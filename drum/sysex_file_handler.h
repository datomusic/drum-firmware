#ifndef DRUM_SYSEX_FILE_HANDLER_H
#define DRUM_SYSEX_FILE_HANDLER_H

#include "drum/applications/rompler/standard_file_ops.h"
#include "drum/configuration_manager.h"
#include "drum/sample_repository.h"
#include "drum/sysex/protocol.h"
#include "etl/observer.h"
#include "musin/hal/logger.h"

extern "C" {
#include "pico/time.h"
}

#include "events.h"
#include "drum/config.h"

namespace drum {

class SysExFileHandler
    : public etl::observable<etl::observer<drum::Events::SysExTransferStateChangeEvent>, drum::config::MAX_SYSEX_EVENT_OBSERVERS> {
public:
  SysExFileHandler(ConfigurationManager &config_manager, SampleRepository &sample_repository,
                   musin::Logger &logger);

  void update(absolute_time_t now);

  sysex::Protocol<StandardFileOps> &get_protocol();
  void on_file_received();

private:
  ConfigurationManager &config_manager_;
  SampleRepository &sample_repository_;
  musin::Logger &logger_;

  StandardFileOps file_ops_;
  sysex::Protocol<StandardFileOps> protocol_;
  bool new_file_received_ = false;
  bool was_busy_ = false;
};

} // namespace drum

#endif // DRUM_SYSEX_FILE_HANDLER_H
