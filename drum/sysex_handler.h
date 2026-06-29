#ifndef DRUM_SYSEX_HANDLER_H
#define DRUM_SYSEX_HANDLER_H

#include "drum/configuration_manager.h"
#include "drum/standard_file_ops.h"
#include "drum/sysex/firmware_update.h"
#include "drum/sysex/protocol.h"
#include "drum/sysex/sds_protocol.h"
#include "etl/observer.h"
#include "musin/flash/firmware_writer.h"
#include "musin/hal/logger.h"

extern "C" {
#include "pico/time.h"
}

#include "drum/config.h"
#include "events.h"

namespace drum {

class SysExHandler
    : public etl::observable<
          etl::observer<drum::Events::SysExTransferStateChangeEvent>,
          drum::config::MAX_SYSEX_EVENT_OBSERVERS> {
public:
  SysExHandler(ConfigurationManager &config_manager, musin::Logger &logger,
               musin::filesystem::Filesystem &filesystem);

  void update(absolute_time_t now);

  /**
   * @brief Handles an incoming raw SysEx message chunk.
   *
   * @param chunk A non-owning view of the SysEx data.
   */
  void handle_sysex_message(const sysex::Chunk &chunk);

  /**
   * @brief Checks if a file transfer is currently in progress.
   */
  bool is_busy() const;

  void on_file_received();

  /**
   * @brief Gates BeginFirmwareUpdate. Disallowed while a trial-booted
   * firmware has not yet committed itself.
   */
  void set_firmware_update_allowed(bool allowed);

private:
  void handle_firmware_update_message(const sysex::Chunk &chunk,
                                      absolute_time_t now);
  void notify_firmware_update_progress();
  void print_firmware_version() const;
  void print_serial_number() const;
  void send_storage_info() const;
  void send_universal_identity_response() const;

  ConfigurationManager &config_manager_;
  musin::Logger &logger_;
  musin::filesystem::Filesystem &filesystem_;

  StandardFileOps file_ops_;
  sysex::Protocol<StandardFileOps> protocol_;
  sds::Protocol<StandardFileOps> sds_protocol_;
  musin::flash::FirmwareWriter firmware_writer_;
  sysex::FirmwareUpdate<musin::flash::FirmwareWriter> firmware_update_;
  bool new_file_received_ = false;
  bool was_busy_ = false;
  std::optional<uint8_t> last_notified_sample_slot_ = std::nullopt;
  bool firmware_update_allowed_ = true;
  bool firmware_reboot_pending_ = false;
  absolute_time_t firmware_reboot_time_{};
  float last_notified_progress_ = -1.0f;
};

} // namespace drum

#endif // DRUM_SYSEX_HANDLER_H
