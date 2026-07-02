#ifndef DRUM_SETTINGS_MANAGER_H
#define DRUM_SETTINGS_MANAGER_H

#include "drum/settings.h"
#include "musin/hal/logger.h"

namespace drum {

/**
 * @brief Persists settings as one file per setting under /settings/.
 *
 * The filename is the setting's short name and the file content is the raw
 * value byte. A missing or unreadable file means the compile-time default
 * applies, so no migration or versioning of the store is needed: unknown
 * files are ignored and absent files fall back to defaults.
 */
class SettingsManager {
public:
  SettingsManager(settings::Settings &settings, musin::Logger &logger);

  /**
   * @brief Loads all known settings from the filesystem.
   * Call once after the filesystem is mounted.
   */
  void init();

  [[nodiscard]] uint8_t get(settings::Id id) const;

  /**
   * @brief Validates, applies and persists a setting value.
   * @return false for unknown ids, out-of-range values, or write failures.
   */
  bool set(settings::Id id, uint8_t value);

private:
  void load_one(const settings::Descriptor &descriptor);
  bool persist_one(const settings::Descriptor &descriptor, uint8_t value);

  settings::Settings &settings_;
  musin::Logger &logger_;
};

} // namespace drum

#endif // DRUM_SETTINGS_MANAGER_H
