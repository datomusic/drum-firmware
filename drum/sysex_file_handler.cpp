#include "drum/sysex_file_handler.h"

namespace drum {

SysExFileHandler::SysExFileHandler(ConfigurationManager &config_manager,
                                   SampleRepository &sample_repository, musin::Logger &logger)
    : config_manager_(config_manager), sample_repository_(sample_repository), logger_(logger),
      file_ops_(logger), protocol_(file_ops_, logger) {
}

void SysExFileHandler::update() {
  if (protocol_.busy()) {
    // The protocol is actively receiving a file.
    // We could add visual feedback here, e.g., pulse a specific LED.
  } else if (new_file_received_) {
    logger_.info("SysExFileHandler: New file received, reloading configuration.");
    if (config_manager_.load()) {
      sample_repository_.load_from_config(config_manager_.get_sample_configs());
    }
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
