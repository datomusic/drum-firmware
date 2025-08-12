#ifndef DRUM_SYSEX_HANDLER_H
#define DRUM_SYSEX_HANDLER_H

#include "drum/applications/rompler/standard_file_ops.h"
#include "drum/configuration_manager.h"
#include "drum/sysex/protocol.h"
#include "etl/observer.h"
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
  SysExHandler(ConfigurationManager &config_manager, musin::Logger &logger);

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

private:
  void print_firmware_version() const;
  void print_serial_number() const;
  void send_storage_info() const;

  ConfigurationManager &config_manager_;
  musin::Logger &logger_;

  StandardFileOps file_ops_;
  sysex::Protocol<StandardFileOps> protocol_;
  bool new_file_received_ = false;
  bool was_busy_ = false;
};

} // namespace drum

#endif // DRUM_SYSEX_HANDLER_H
