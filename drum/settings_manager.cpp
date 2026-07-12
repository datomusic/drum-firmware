#include "drum/settings_manager.h"

#include "etl/span.h"

namespace drum {

SettingsManager::SettingsManager(settings::Settings &settings,
                                 musin::Logger &logger)
    : store_(settings),
      impl_(etl::span<const musin::settings::Descriptor>(
                settings::DESCRIPTORS.begin(), settings::DESCRIPTORS.end()),
            store_, logger) {
}

void SettingsManager::init() {
  impl_.init();
}

uint8_t SettingsManager::get(settings::Id id) const {
  return impl_.get(static_cast<uint8_t>(id));
}

bool SettingsManager::set(settings::Id id, uint8_t value) {
  return impl_.set(static_cast<uint8_t>(id), value);
}

} // namespace drum
