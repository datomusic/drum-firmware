#include "drum/sysex_handler.h"
#include "drum/sequencer_controller.h"
#include "drum/sysex/sequencer_state_codec.h"
#include "events.h"
#include "musin/midi/midi_wrapper.h"
#include "version.h"
#include <pico/bootrom.h>
#include <pico/unique_id.h>

namespace drum {

SysExHandler::SysExHandler(ConfigurationManager &config_manager,
                           musin::Logger &logger,
                           musin::filesystem::Filesystem &filesystem)
    : config_manager_(config_manager), logger_(logger), filesystem_(filesystem),
      file_ops_(logger, filesystem), protocol_(file_ops_, logger),
      sds_protocol_(file_ops_, logger) {
}

void SysExHandler::update(absolute_time_t now) {
  protocol_.check_timeout(now);
  bool current_busy_state = is_busy();

  // Get current sample slot if available
  std::optional<uint8_t> current_sample_slot = std::nullopt;
  if (auto slot_opt = sds_protocol_.current_sample_number_opt();
      slot_opt.has_value()) {
    current_sample_slot = static_cast<uint8_t>(slot_opt.value() & 0x7F);
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
          .is_active = false, .sample_slot = std::nullopt};
      this->notify_observers(event);
      last_notified_sample_slot_ = std::nullopt;
    }
    was_busy_ = current_busy_state;
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
    auto result = sds_protocol_.process_message(sds_payload, sds_sender,
                                                get_absolute_time());

    switch (result) {
    case sds::Result::SampleComplete:
      logger_.info("SDS: Sample transfer completed successfully");
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
    const auto payload_start = chunk.cbegin() + 5;
    const auto payload = etl::span<const uint8_t>{payload_start, chunk.cend()};
    handle_set_sequencer_state(payload);
    break;
  }
  default:
    // Other results are handled internally by the protocol or are errors.
    break;
  }
}

bool SysExHandler::is_busy() const {
  return protocol_.busy() || sds_protocol_.is_busy();
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

void SysExHandler::set_sequencer_controller(void *controller) {
  sequencer_controller_ = controller;
}

void SysExHandler::send_sequencer_state() const {
  if (!sequencer_controller_) {
    logger_.error("SysEx: Cannot send sequencer state - controller not set");
    return;
  }

  logger_.info("Sending sequencer state via SysEx");

  auto *controller = static_cast<
      SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> *>(
      sequencer_controller_);
  const auto state = controller->get_current_state();

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
  if (!sequencer_controller_) {
    logger_.error("SysEx: Cannot set sequencer state - controller not set");
    uint8_t nack[] = {
        0xF0,
        drum::config::sysex::MANUFACTURER_ID_0,
        drum::config::sysex::MANUFACTURER_ID_1,
        drum::config::sysex::MANUFACTURER_ID_2,
        drum::config::sysex::DEVICE_ID,
        static_cast<uint8_t>(sysex::Protocol<StandardFileOps>::Tag::Nack),
        0xF7};
    MIDI::sendSysEx(sizeof(nack), nack);
    return;
  }

  logger_.info("Received set sequencer state command");

  const auto maybe_state = sysex::decode_sequencer_state(payload);
  if (!maybe_state.has_value()) {
    logger_.error("SysEx: Failed to decode sequencer state payload");
    uint8_t nack[] = {
        0xF0,
        drum::config::sysex::MANUFACTURER_ID_0,
        drum::config::sysex::MANUFACTURER_ID_1,
        drum::config::sysex::MANUFACTURER_ID_2,
        drum::config::sysex::DEVICE_ID,
        static_cast<uint8_t>(sysex::Protocol<StandardFileOps>::Tag::Nack),
        0xF7};
    MIDI::sendSysEx(sizeof(nack), nack);
    return;
  }

  auto *controller = static_cast<
      SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> *>(
      sequencer_controller_);
  if (!controller->apply_state(maybe_state.value())) {
    logger_.error("SysEx: Failed to apply sequencer state");
    uint8_t nack[] = {
        0xF0,
        drum::config::sysex::MANUFACTURER_ID_0,
        drum::config::sysex::MANUFACTURER_ID_1,
        drum::config::sysex::MANUFACTURER_ID_2,
        drum::config::sysex::DEVICE_ID,
        static_cast<uint8_t>(sysex::Protocol<StandardFileOps>::Tag::Nack),
        0xF7};
    MIDI::sendSysEx(sizeof(nack), nack);
    return;
  }

  logger_.info("SysEx: Sequencer state applied successfully");
  uint8_t ack[] = {
      0xF0,
      drum::config::sysex::MANUFACTURER_ID_0,
      drum::config::sysex::MANUFACTURER_ID_1,
      drum::config::sysex::MANUFACTURER_ID_2,
      drum::config::sysex::DEVICE_ID,
      static_cast<uint8_t>(sysex::Protocol<StandardFileOps>::Tag::Ack),
      0xF7};
  MIDI::sendSysEx(sizeof(ack), ack);
}

} // namespace drum
