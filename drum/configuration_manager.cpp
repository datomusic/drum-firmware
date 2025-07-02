#include "drum/configuration_manager.h"
#include "kit_definitions.h"
#include <cstdio>

namespace drum {

ConfigurationManager::ConfigurationManager(musin::Logger &logger) : logger_(logger) {
}

bool ConfigurationManager::load() {
  logger_.info("Loading configuration from /kit.bin");
  FILE *config_file = fopen("/kit.bin", "rb");
  if (!config_file) {
    logger_.info("Could not open /kit.bin. Loading factory kit.");
    return load_factory_kit();
  }

  static etl::array<config::SampleSlotMetadata, 32> buffer;
  size_t items_read = fread(buffer.data(), sizeof(config::SampleSlotMetadata), buffer.size(), config_file);
  fclose(config_file);

  if (items_read == 0) {
    logger_.warn("/kit.bin is empty. Loading factory kit.");
    return load_factory_kit();
  }

  if (items_read != buffer.size()) {
    logger_.warn("Partial read from /kit.bin. Loading factory kit.");
    return load_factory_kit();
  }

  sample_configs_.clear();
  for(const auto& item : buffer) {
    SampleConfig cfg;
    cfg.slot = &item - &buffer[0];
    cfg.note = item.midi_note;
    cfg.track = item.track;
    cfg.color = (static_cast<uint32_t>(item.color.r) << 16) |
                (static_cast<uint32_t>(item.color.g) << 8) |
                (static_cast<uint32_t>(item.color.b));
    sample_configs_.push_back(cfg);
  }

  return true;
}

bool ConfigurationManager::load_factory_kit() {
    logger_.info("Loading factory kit from flash.");
    FILE* factory_kit_file = fopen("/factory_kit.bin", "rb");
    if (!factory_kit_file) {
        logger_.error("Could not open /factory_kit.bin.");
        return false;
    }

    static etl::array<config::SampleSlotMetadata, 32> buffer;
    size_t items_read = fread(buffer.data(), sizeof(config::SampleSlotMetadata), buffer.size(), factory_kit_file);
    fclose(factory_kit_file);

    if (items_read != buffer.size()) {
        logger_.error("Failed to read factory kit.");
        return false;
    }

    sample_configs_.clear();
    for(const auto& item : buffer) {
        SampleConfig cfg;
        cfg.slot = &item - &buffer[0];
        cfg.note = item.midi_note;
        cfg.track = item.track;
        cfg.color = (static_cast<uint32_t>(item.color.r) << 16) |
                    (static_cast<uint32_t>(item.color.g) << 8) |
                    (static_cast<uint32_t>(item.color.b));
        sample_configs_.push_back(cfg);
    }

    return true;
}


const etl::ivector<SampleConfig> &ConfigurationManager::get_sample_configs() const {
  return sample_configs_;
}

} // namespace drum

