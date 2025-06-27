#include "drum/sample_repository.h"
#include "drum/configuration_manager.h" // For SampleConfig definition
#include "drum/sample_repository.h"
#include "etl/array.h"

namespace drum {

SampleRepository::SampleRepository(musin::Logger &logger) : logger_(logger) {
}

void SampleRepository::load_from_config(const etl::ivector<SampleConfig> &sample_configs) {
  // Clear any existing paths before loading.
  for (auto &path_opt : sample_paths_) {
    path_opt.reset();
  }

  logger_.info("Loading sample paths from configuration...");

  for (const auto &config : sample_configs) {
    if (config.slot < MAX_SAMPLES) {
      sample_paths_[config.slot].emplace(config.path);
      logger_.info("  - Slot", config.slot);
      logger_.info(config.path.c_str());
    } else {
      logger_.warn("Ignoring sample with out-of-bounds slot", config.slot);
    }
  }
}

etl::optional<etl::string_view> SampleRepository::get_path(size_t index) const {
  if (index >= MAX_SAMPLES || !sample_paths_[index].has_value()) {
    return etl::nullopt;
  }
  return etl::string_view(sample_paths_[index].value());
}

} // namespace drum
