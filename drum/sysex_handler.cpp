#include "drum/sysex_handler.h"
#include "drum/sample_slot_manager.h"
#include "drum/sysex/sequencer_state_codec.h"
#include "etl/algorithm.h"
#include "etl/array.h"
#include "events.h"
#include "musin/midi/midi_wrapper.h"
#include "version.h"
#include <boot/picoboot_constants.h>
#include <hardware/regs/addressmap.h>
#include <pico/bootrom.h>
#include <pico/unique_id.h>

namespace drum {

namespace {
// Wraps an SDS payload (message type onward) in F0 7E <channel> ... F7.
void send_sds_message(const etl::span<const uint8_t> &payload) {
  etl::array<uint8_t, 128> msg;
  msg[0] = 0xF0;
  msg[1] = 0x7E;
  msg[2] = 0x65;
  etl::copy(payload.begin(), payload.end(), msg.begin() + 3);
  msg[3 + payload.size()] = 0xF7;
  MIDI::sendSysEx(payload.size() + 4, msg.data());
}
} // namespace

SysExHandler::SysExHandler(ConfigurationManager &config_manager,
                           SettingsManager &settings_manager,
                           musin::Logger &logger,
                           musin::filesystem::Filesystem &filesystem)
    : config_manager_(config_manager), settings_manager_(settings_manager),
      logger_(logger), filesystem_(filesystem), file_ops_(logger, filesystem),
      protocol_(file_ops_, logger), sds_protocol_(file_ops_, logger),
      sds_dump_sender_(file_ops_, logger), firmware_writer_(logger),
      firmware_update_(firmware_writer_, logger) {
}

void SysExHandler::update(absolute_time_t now) {
  protocol_.check_timeout(now);
  sds_protocol_.check_timeout(now);
  sds_dump_sender_.update(send_sds_message, now);
  firmware_update_.check_timeout(now);

  if (firmware_reboot_pending_ &&
      absolute_time_diff_us(firmware_reboot_time_, now) > 0) {
    if (const auto target = firmware_writer_.target_flash_offset();
        target.has_value()) {
      logger_.info("SysEx: Rebooting into new firmware for trial boot.");
      rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE |
                     REBOOT2_FLAG_NO_RETURN_ON_SUCCESS,
                 100, XIP_BASE + target.value(), 0);
    }
    firmware_reboot_pending_ = false;
  }

  bool current_busy_state = is_busy();

  // Get current sample slot if available
  std::optional<uint8_t> current_sample_slot = std::nullopt;
  if (auto slot_opt = sds_protocol_.current_sample_number_opt();
      slot_opt.has_value()) {
    current_sample_slot = static_cast<uint8_t>(slot_opt.value() & 0x7F);
  } else if (auto dump_slot = sds_dump_sender_.current_sample_number_opt();
             dump_slot.has_value()) {
    current_sample_slot = static_cast<uint8_t>(dump_slot.value() & 0x7F);
  }

  // Check for busy state changes (transfer start/stop)
  if (current_busy_state != was_busy_) {
    if (current_busy_state) {
      if (current_sample_slot.has_value()) {
        logger_.info("SysEx file transfer started, sample slot:",
                     static_cast<uint32_t>(current_sample_slot.value()));
      } else {
        logger_.info("SysEx file transfer started.");
      }
      drum::Events::SysExTransferStateChangeEvent event{
          .is_active = true, .sample_slot = current_sample_slot};
      this->notify_observers(event);
      last_notified_sample_slot_ = current_sample_slot;
    } else {
      logger_.info("SysEx file transfer finished.");
      drum::Events::SysExTransferStateChangeEvent event{
          .is_active = false,
          .sample_slot = last_notified_sample_slot_,
          .skip_debounce = sds_dump_was_active_};
      this->notify_observers(event);
      last_notified_sample_slot_ = std::nullopt;
      sds_dump_was_active_ = false;
    }
    was_busy_ = current_busy_state;
  }
  // Report progress during an active firmware update
  else if (current_busy_state && firmware_update_.busy()) {
    notify_firmware_update_progress();
  }
  // Check for sample slot changes during active transfer
  else if (current_busy_state &&
           current_sample_slot != last_notified_sample_slot_) {
    if (current_sample_slot.has_value()) {
      logger_.info("SysEx sample slot updated:",
                   static_cast<uint32_t>(current_sample_slot.value()));
    } else {
      logger_.info("SysEx sample slot cleared");
    }
    drum::Events::SysExTransferStateChangeEvent event{
        .is_active = true, .sample_slot = current_sample_slot};
    this->notify_observers(event);
    last_notified_sample_slot_ = current_sample_slot;
  }

  if (new_file_received_) {
    logger_.info("SysExHandler: New file received, reloading configuration.");
    config_manager_.load();
    new_file_received_ = false;
  }
}

void SysExHandler::handle_sysex_message(const sysex::Chunk &chunk) {
  logger_.debug("SysEx: Received message, size:",
                static_cast<uint32_t>(chunk.size()));

  // Check if this is a universal SysEx identity request (0x7E 0x7F 0x06 0x01)
  if (chunk.size() >= 4 && chunk[0] == 0x7E && chunk[1] == 0x7F &&
      chunk[2] == 0x06 && chunk[3] == 0x01) {
    logger_.info("Universal SysEx identity request received");
    send_universal_identity_response();
    return;
  }

  // Check if this is an SDS message (starts with 0x7E)
  if (chunk.size() >= 3 && chunk[0] == 0x7E && chunk[1] == 0x65) {
    // SDS message - route to SDS protocol
    auto sds_sender = [this](sds::MessageType type, uint8_t packet_num) {
      uint8_t msg[] = {0xF0,       0x7E, 0x65, static_cast<uint8_t>(type),
                       packet_num, 0xF7};
      MIDI::sendSysEx(sizeof(msg), msg);
    };

    // Extract SDS payload (skip 0x7E and channel)
    const auto sds_payload =
        etl::span<const uint8_t>{chunk.cbegin() + 2, chunk.cend()};
    const uint8_t sds_type = sds_payload[0];

    // Dump requests and, while a dump is active, host handshake responses go
    // to the dump sender. Everything else is the receive path below.
    if (sds_type == sds::DUMP_REQUEST) {
      if (sds_protocol_.is_busy()) {
        logger_.warn("SDS: Dump request refused during sample upload");
        const etl::array<uint8_t, 2> cancel{sds::CANCEL, 0};
        send_sds_message(etl::span<const uint8_t>{cancel});
      } else {
        sds_dump_sender_.handle_dump_request(sds_payload, send_sds_message,
                                             get_absolute_time());
        sds_dump_was_active_ = true;
      }
      return;
    }
    if (sds_dump_sender_.is_busy() &&
        (sds_type == sds::ACK || sds_type == sds::NAK ||
         sds_type == sds::WAIT || sds_type == sds::CANCEL)) {
      sds_dump_sender_.handle_response(sds_type, send_sds_message,
                                       get_absolute_time());
      return;
    }

    if (sds_dump_sender_.is_busy()) {
      logger_.warn("SDS: Upload message refused during sample download");
      const etl::array<uint8_t, 2> nak{sds::NAK, 0};
      send_sds_message(etl::span<const uint8_t>{nak});
      return;
    }

    // Capture the active slot before processing: a completing data packet
    // returns the protocol to Idle, after which the slot is no longer
    // reported.
    const auto active_sample_slot = sds_protocol_.current_sample_number_opt();
    auto result = sds_protocol_.process_message(sds_payload, sds_sender,
                                                get_absolute_time());

    switch (result) {
    case sds::Result::SampleComplete:
      logger_.info("SDS: Sample transfer completed successfully");
      if (sample_slot_manager_ != nullptr && active_sample_slot.has_value()) {
        sample_slot_manager_->invalidate_sample(active_sample_slot.value());
      }
      on_file_received();
      break;
    case sds::Result::ChecksumError:
      logger_.warn("SDS: Checksum error in received packet");
      break;
    case sds::Result::FileError:
      logger_.error("SDS: File operation failed");
      break;
    default:
      // Other results are handled internally
      break;
    }
    return;
  }

  // Firmware update messages have their own state machine
  if (sysex::FirmwareUpdate<musin::flash::FirmwareWriter>::claims(chunk)) {
    handle_firmware_update_message(chunk, get_absolute_time());
    return;
  }

  // Not an SDS message - route to existing custom protocol
  auto sender = [this](sysex::Protocol<StandardFileOps>::Tag tag) {
    uint8_t msg[] = {0xF0,
                     drum::config::sysex::MANUFACTURER_ID_0,
                     drum::config::sysex::MANUFACTURER_ID_1,
                     drum::config::sysex::MANUFACTURER_ID_2,
                     drum::config::sysex::DEVICE_ID,
                     static_cast<uint8_t>(tag),
                     0xF7};
    MIDI::sendSysEx(sizeof(msg), msg);
  };

  auto result = protocol_.template handle_chunk<decltype(sender)>(
      chunk, sender, get_absolute_time());

  switch (result) {
  case sysex::Protocol<StandardFileOps>::Result::FileWritten:
    on_file_received();
    break;
  case sysex::Protocol<StandardFileOps>::Result::Reboot:
    reset_usb_boot(0, 0);
    break;
  case sysex::Protocol<StandardFileOps>::Result::PrintFirmwareVersion:
    print_firmware_version();
    break;
  case sysex::Protocol<StandardFileOps>::Result::PrintSerialNumber:
    print_serial_number();
    break;
  case sysex::Protocol<StandardFileOps>::Result::PrintStorageInfo:
    send_storage_info();
    break;
  case sysex::Protocol<StandardFileOps>::Result::PrintSequencerState:
    send_sequencer_state();
    break;
  case sysex::Protocol<StandardFileOps>::Result::SetSequencerState: {
    const auto payload_start =
        chunk.cbegin() + sysex::SYSEX_CHUNK_PAYLOAD_OFFSET;
    const auto payload = etl::span<const uint8_t>{payload_start, chunk.cend()};
    handle_set_sequencer_state(payload);
    break;
  }
  case sysex::Protocol<StandardFileOps>::Result::GetSetting: {
    const auto payload_start =
        chunk.cbegin() + sysex::SYSEX_CHUNK_PAYLOAD_OFFSET;
    const auto payload = etl::span<const uint8_t>{payload_start, chunk.cend()};
    send_setting_value(payload);
    break;
  }
  case sysex::Protocol<StandardFileOps>::Result::SetSetting: {
    const auto payload_start =
        chunk.cbegin() + sysex::SYSEX_CHUNK_PAYLOAD_OFFSET;
    const auto payload = etl::span<const uint8_t>{payload_start, chunk.cend()};
    handle_set_setting(payload);
    break;
  }
  default:
    // Other results are handled internally by the protocol or are errors.
    break;
  }
}

bool SysExHandler::is_busy() const {
  return protocol_.busy() || sds_protocol_.is_busy() ||
         sds_dump_sender_.is_busy() || firmware_update_.busy();
}

void SysExHandler::set_firmware_update_allowed(bool allowed) {
  firmware_update_allowed_ = allowed;
}

void SysExHandler::notify_firmware_update_progress() {
  if (firmware_update_.total_size() == 0) {
    return;
  }
  const float progress = static_cast<float>(firmware_update_.bytes_received()) /
                         static_cast<float>(firmware_update_.total_size());
  if (progress - last_notified_progress_ < 1.0f / 32.0f) {
    return;
  }
  last_notified_progress_ = progress;
  drum::Events::SysExTransferStateChangeEvent event{
      .is_active = true, .sample_slot = std::nullopt, .progress = progress};
  this->notify_observers(event);
}

void SysExHandler::handle_firmware_update_message(const sysex::Chunk &chunk,
                                                  absolute_time_t now) {
  auto sender = [](uint8_t tag) {
    uint8_t msg[] = {0xF0,
                     drum::config::sysex::MANUFACTURER_ID_0,
                     drum::config::sysex::MANUFACTURER_ID_1,
                     drum::config::sysex::MANUFACTURER_ID_2,
                     drum::config::sysex::DEVICE_ID,
                     tag,
                     0xF7};
    MIDI::sendSysEx(sizeof(msg), msg);
  };

  using Update = sysex::FirmwareUpdate<musin::flash::FirmwareWriter>;

  if (chunk[4] == Update::Tag::BeginFirmwareUpdate &&
      !firmware_update_allowed_) {
    logger_.warn("SysEx: Firmware update refused during unbought trial boot.");
    sender(Update::Tag::Nack);
    return;
  }

  const auto result = firmware_update_.handle_chunk(chunk, sender, now);
  if (result == Update::Result::UpdateReady) {
    // Give the Ack time to drain through the MIDI output queue, then reboot
    // into the new image for its trial boot.
    firmware_reboot_pending_ = true;
    firmware_reboot_time_ = make_timeout_time_ms(500);
  }
  if (chunk[4] == Update::Tag::BeginFirmwareUpdate) {
    last_notified_progress_ = -1.0f;
  }
}

void SysExHandler::on_file_received() {
  new_file_received_ = true;
}

void SysExHandler::print_firmware_version() const {
  logger_.info("Sending firmware version via SysEx");
  static constexpr uint8_t sysex[] = {
      0xF0,
      drum::config::sysex::MANUFACTURER_ID_0,
      drum::config::sysex::MANUFACTURER_ID_1,
      drum::config::sysex::MANUFACTURER_ID_2,
      drum::config::sysex::DEVICE_ID,
      static_cast<uint8_t>(
          sysex::Protocol<
              StandardFileOps>::Tag::RequestFirmwareVersion), // Command byte
      (uint8_t)(FIRMWARE_MAJOR & 0x7F),
      (uint8_t)(FIRMWARE_MINOR & 0x7F),
      (uint8_t)(FIRMWARE_PATCH & 0x7F),
      0xF7};
  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void SysExHandler::print_serial_number() const {
  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id);

  uint8_t sysex[16];
  sysex[0] = 0xF0;
  sysex[1] = drum::config::sysex::MANUFACTURER_ID_0;
  sysex[2] = drum::config::sysex::MANUFACTURER_ID_1;
  sysex[3] = drum::config::sysex::MANUFACTURER_ID_2;
  sysex[4] = drum::config::sysex::DEVICE_ID;
  sysex[5] = static_cast<uint8_t>(
      sysex::Protocol<StandardFileOps>::Tag::RequestSerialNumber);

  uint8_t msbs = 0;
  for (int i = 0; i < 8; ++i) {
    sysex[6 + i] = id.id[i] & 0x7F;
    msbs |= ((id.id[i] >> 7) & 0x01) << i;
  }
  sysex[14] = msbs;
  sysex[15] = 0xF7;

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void SysExHandler::send_storage_info() const {
  logger_.info("Sending storage info via SysEx");
  musin::filesystem::StorageInfo info = filesystem_.get_storage_info();
  logger_.info("Total:", info.total_bytes);
  logger_.info("Free:", info.free_bytes);

  uint8_t sysex[16];
  sysex[0] = 0xF0;
  sysex[1] = drum::config::sysex::MANUFACTURER_ID_0;
  sysex[2] = drum::config::sysex::MANUFACTURER_ID_1;
  sysex[3] = drum::config::sysex::MANUFACTURER_ID_2;
  sysex[4] = drum::config::sysex::DEVICE_ID;
  sysex[5] = static_cast<uint8_t>(
      sysex::Protocol<StandardFileOps>::Tag::StorageInfoResponse);

  // Total space (32-bit)
  sysex[6] = (info.total_bytes >> 21) & 0x7F;
  sysex[7] = (info.total_bytes >> 14) & 0x7F;
  sysex[8] = (info.total_bytes >> 7) & 0x7F;
  sysex[9] = info.total_bytes & 0x7F;

  // Free space (32-bit)
  sysex[10] = (info.free_bytes >> 21) & 0x7F;
  sysex[11] = (info.free_bytes >> 14) & 0x7F;
  sysex[12] = (info.free_bytes >> 7) & 0x7F;
  sysex[13] = info.free_bytes & 0x7F;

  sysex[14] = 0;
  sysex[15] = 0xF7;

  MIDI::sendSysEx(sizeof(sysex), sysex);
}

void SysExHandler::set_sequencer_state_access(
    SequencerStateAccess *sequencer_state_access) {
  sequencer_state_access_ = sequencer_state_access;
}

void SysExHandler::set_sample_slot_manager(
    SampleSlotManager *sample_slot_manager) {
  sample_slot_manager_ = sample_slot_manager;
}

void SysExHandler::send_sequencer_state() const {
  if (!sequencer_state_access_) {
    logger_.error("SysEx: Cannot send sequencer state - accessor not set");
    return;
  }

  logger_.info("Sending sequencer state via SysEx");

  const auto state = sequencer_state_access_->get_current_state();

  etl::array<uint8_t, sysex::SEQUENCER_STATE_PAYLOAD_SIZE> payload;
  const size_t encoded_size =
      sysex::encode_sequencer_state(state, etl::span{payload});

  if (encoded_size != sysex::SEQUENCER_STATE_PAYLOAD_SIZE) {
    logger_.error("SysEx: Failed to encode sequencer state");
    return;
  }

  uint8_t message[sysex::SYSEX_HEADER_SIZE +
                  sysex::SEQUENCER_STATE_PAYLOAD_SIZE + sysex::SYSEX_END_SIZE];
  message[0] = 0xF0;
  message[1] = drum::config::sysex::MANUFACTURER_ID_0;
  message[2] = drum::config::sysex::MANUFACTURER_ID_1;
  message[3] = drum::config::sysex::MANUFACTURER_ID_2;
  message[4] = drum::config::sysex::DEVICE_ID;
  message[5] = static_cast<uint8_t>(
      sysex::Protocol<StandardFileOps>::Tag::SequencerStateResponse);

  for (size_t i = 0; i < encoded_size; ++i) {
    message[sysex::SYSEX_HEADER_SIZE + i] = payload[i];
  }
  message[sysex::SYSEX_HEADER_SIZE + encoded_size] = 0xF7;

  MIDI::sendSysEx(sizeof(message), message);
}

void SysExHandler::handle_set_sequencer_state(
    const etl::span<const uint8_t> &payload) {
  auto sender = [](sysex::Protocol<StandardFileOps>::Tag tag) {
    uint8_t msg[] = {0xF0,
                     drum::config::sysex::MANUFACTURER_ID_0,
                     drum::config::sysex::MANUFACTURER_ID_1,
                     drum::config::sysex::MANUFACTURER_ID_2,
                     drum::config::sysex::DEVICE_ID,
                     static_cast<uint8_t>(tag),
                     0xF7};
    MIDI::sendSysEx(sizeof(msg), msg);
  };

  if (!sequencer_state_access_) {
    logger_.error("SysEx: Cannot set sequencer state - accessor not set");
    sender(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  logger_.info("Received set sequencer state command");

  const auto maybe_state = sysex::decode_sequencer_state(payload);
  if (!maybe_state.has_value()) {
    logger_.error("SysEx: Failed to decode sequencer state payload");
    sender(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  if (!sequencer_state_access_->apply_state(maybe_state.value())) {
    logger_.error("SysEx: Failed to apply sequencer state");
    sender(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  logger_.info("SysEx: Sequencer state applied successfully");
  sender(sysex::Protocol<StandardFileOps>::Tag::Ack);
}

namespace {
void send_reply_tag(sysex::Protocol<StandardFileOps>::Tag tag) {
  uint8_t msg[] = {0xF0,
                   drum::config::sysex::MANUFACTURER_ID_0,
                   drum::config::sysex::MANUFACTURER_ID_1,
                   drum::config::sysex::MANUFACTURER_ID_2,
                   drum::config::sysex::DEVICE_ID,
                   static_cast<uint8_t>(tag),
                   0xF7};
  MIDI::sendSysEx(sizeof(msg), msg);
}
} // namespace

void SysExHandler::send_setting_value(
    const etl::span<const uint8_t> &payload) const {
  if (payload.empty()) {
    logger_.error("SysEx: GetSetting without setting id");
    send_reply_tag(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  const auto id = static_cast<settings::Id>(payload[0]);
  if (settings::find_descriptor(id) == nullptr) {
    logger_.warn("SysEx: GetSetting for unknown id",
                 static_cast<uint32_t>(payload[0]));
    send_reply_tag(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  uint8_t message[] = {
      0xF0,
      drum::config::sysex::MANUFACTURER_ID_0,
      drum::config::sysex::MANUFACTURER_ID_1,
      drum::config::sysex::MANUFACTURER_ID_2,
      drum::config::sysex::DEVICE_ID,
      static_cast<uint8_t>(sysex::Protocol<StandardFileOps>::Tag::SettingValue),
      payload[0],
      settings_manager_.get(id),
      0xF7};
  MIDI::sendSysEx(sizeof(message), message);
}

void SysExHandler::handle_set_setting(const etl::span<const uint8_t> &payload) {
  if (payload.size() < 2) {
    logger_.error("SysEx: SetSetting payload too short");
    send_reply_tag(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  const auto id = static_cast<settings::Id>(payload[0]);
  if (!settings_manager_.set(id, payload[1])) {
    send_reply_tag(sysex::Protocol<StandardFileOps>::Tag::Nack);
    return;
  }

  logger_.info("SysEx: Setting applied", static_cast<uint32_t>(payload[0]));
  send_reply_tag(sysex::Protocol<StandardFileOps>::Tag::Ack);
}

void SysExHandler::send_universal_identity_response() const {
  logger_.info("Sending universal SysEx identity response");

  // MIDI Universal Identity Response format:
  // F0 7E 7F 06 02 <manufacturer_id> <device_family_LSB> <device_family_MSB>
  // <device_member_LSB> <device_member_MSB> <software_revision> F7
  static constexpr uint8_t sysex[] = {
      0xF0,                                   // SysEx start
      0x7E,                                   // Universal Non-Real Time
      0x7F,                                   // Device ID (7F = all devices)
      0x06,                                   // General Information sub-ID1
      0x02,                                   // Identity Response sub-ID2
      drum::config::sysex::MANUFACTURER_ID_0, // Dato manufacturer ID
      drum::config::sysex::MANUFACTURER_ID_1,
      drum::config::sysex::MANUFACTURER_ID_2,
      0x02, // Device family LSB (2nd product family)
      0x00, // Device family MSB
      0x01, // Device family member LSB (1st product)
      0x00, // Device family member MSB
      (uint8_t)(FIRMWARE_MAJOR & 0x7F), // Software revision (firmware version)
      (uint8_t)(FIRMWARE_MINOR & 0x7F),
      (uint8_t)(FIRMWARE_PATCH & 0x7F),
      0xF7 // SysEx end
  };
  MIDI::sendSysEx(sizeof(sysex), sysex);
}

} // namespace drum
