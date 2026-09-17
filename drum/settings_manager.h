#ifndef DRUM_SETTINGS_MANAGER_H
#define DRUM_SETTINGS_MANAGER_H

#include "drum/settings.h"
#include "musin/hal/logger.h"
#include "musin/settings/settings_manager.h"

namespace drum {

/**
 * @brief DRUM's settings persistence: the generic musin SettingsManager
 * bound to the DRUM descriptor table and Settings store.
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
  class Store final : public musin::settings::ValueStore {
  public:
    explicit Store(settings::Settings &settings) : settings_(settings) {
    }

    bool set(uint8_t id, uint8_t value) override {
      return settings_.set(static_cast<settings::Id>(id), value);
    }

    [[nodiscard]] uint8_t get(uint8_t id) const override {
      return settings_.get(static_cast<settings::Id>(id));
    }

  private:
    settings::Settings &settings_;
  };

  Store store_;
  musin::settings::SettingsManager impl_;
};

} // namespace drum

#endif // DRUM_SETTINGS_MANAGER_H
