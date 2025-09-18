#ifndef DRUM_SYSEX_HANDLER_H
#define DRUM_SYSEX_HANDLER_H

#include "drum/configuration_manager.h"
#include "drum/standard_file_ops.h"
#include "drum/sysex/firmware_update_protocol.h"
#include "drum/sysex/protocol.h"
#include "drum/sysex/sds_protocol.h"
#include "etl/observer.h"
#include "etl/optional.h"
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

  void set_firmware_targets(
      drum::firmware::FirmwarePartitionManager &partition_manager,
      drum::firmware::PartitionFlashWriter &flash_writer);

  void on_file_received();

private:
  void print_firmware_version() const;
  void print_serial_number() const;
  void send_storage_info() const;

  ConfigurationManager &config_manager_;
  musin::Logger &logger_;
  musin::filesystem::Filesystem &filesystem_;

  StandardFileOps file_ops_;
  sysex::Protocol<StandardFileOps> protocol_;
  sds::Protocol<StandardFileOps> sds_protocol_;
  etl::optional<drum::firmware::FirmwareUpdateProtocol> firmware_protocol_;
  bool new_file_received_ = false;
  bool was_busy_ = false;
};

} // namespace drum

#endif // DRUM_SYSEX_HANDLER_H
