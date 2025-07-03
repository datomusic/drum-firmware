#include "drum/sysex_file_handler.h"
#include "events.h"

namespace drum {

SysExFileHandler::SysExFileHandler(ConfigurationManager &config_manager, musin::Logger &logger)
    : config_manager_(config_manager), logger_(logger), file_ops_(logger),
      protocol_(file_ops_, logger) {
}

void SysExFileHandler::update(absolute_time_t now) {
  protocol_.check_timeout(now);
  bool is_busy = protocol_.busy();
  if (is_busy != was_busy_) {
    if (is_busy) {
      logger_.info("SysEx file transfer started.");
      drum::Events::SysExTransferStateChangeEvent event{.is_active = true};
      this->notify_observers(event);
    } else {
      logger_.info("SysEx file transfer finished.");
      drum::Events::SysExTransferStateChangeEvent event{.is_active = false};
      this->notify_observers(event);
    }
    was_busy_ = is_busy;
  }

  if (new_file_received_) {
    logger_.info("SysExFileHandler: New file received, reloading configuration.");
    config_manager_.load();
    new_file_received_ = false;
  }
}

sysex::Protocol<StandardFileOps> &SysExFileHandler::get_protocol() {
  return protocol_;
}

void SysExFileHandler::on_file_received() {
  new_file_received_ = true;
}

} // namespace drum
