#include "drum/sysex_handler.h"
#include "events.h"
#include "musin/midi/midi_wrapper.h"
#include "version.h"
#include <pico/bootrom.h>
#include <pico/unique_id.h>

namespace drum {

SysExHandler::SysExHandler(ConfigurationManager &config_manager,
                           musin::Logger &logger)
    : config_manager_(config_manager), logger_(logger), file_ops_(logger),
      protocol_(file_ops_, logger) {
}

void SysExHandler::update(absolute_time_t now) {
  protocol_.check_timeout(now);
  bool current_busy_state = is_busy();
  if (current_busy_state != was_busy_) {
    if (current_busy_state) {
      logger_.info("SysEx file transfer started.");
      drum::Events::SysExTransferStateChangeEvent event{.is_active = true};
      this->notify_observers(event);
    } else {
      logger_.info("SysEx file transfer finished.");
      drum::Events::SysExTransferStateChangeEvent event{.is_active = false};
      this->notify_observers(event);
    }
    was_busy_ = current_busy_state;
  }

  if (new_file_received_) {
    logger_.info("SysExHandler: New file received, reloading configuration.");
    config_manager_.load();
    new_file_received_ = false;
  }
}

void SysExHandler::handle_sysex_message(const sysex::Chunk &chunk) {
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
  default:
    // Other results are handled internally by the protocol or are errors.
    break;
  }
}

bool SysExHandler::is_busy() const {
  return protocol_.busy();
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
  musin::filesystem::StorageInfo info = musin::filesystem::get_storage_info();
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

} // namespace drum
