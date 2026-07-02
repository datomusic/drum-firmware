#include "drum/settings_manager.h"

#include <cstdio>
#include <sys/stat.h>

namespace drum {

namespace {
constexpr const char *SETTINGS_DIR = "/settings";
constexpr size_t MAX_PATH_LENGTH = 64;

bool build_path(const settings::Descriptor &descriptor,
                char (&path)[MAX_PATH_LENGTH]) {
  const int written = snprintf(path, MAX_PATH_LENGTH, "%s/%.*s", SETTINGS_DIR,
                               static_cast<int>(descriptor.name.size()),
                               descriptor.name.data());
  return written > 0 && static_cast<size_t>(written) < MAX_PATH_LENGTH;
}
} // namespace

SettingsManager::SettingsManager(settings::Settings &settings,
                                 musin::Logger &logger)
    : settings_(settings), logger_(logger) {
}

void SettingsManager::init() {
  mkdir(SETTINGS_DIR, 0777); // Already existing is fine.

  for (const auto &descriptor : settings::DESCRIPTORS) {
    load_one(descriptor);
  }
}

uint8_t SettingsManager::get(settings::Id id) const {
  return settings_.get(id);
}

bool SettingsManager::set(settings::Id id, uint8_t value) {
  const auto *descriptor = settings::find_descriptor(id);
  if (descriptor == nullptr) {
    logger_.warn("Settings: Unknown setting id",
                 static_cast<uint32_t>(static_cast<uint8_t>(id)));
    return false;
  }
  if (!settings_.set(id, value)) {
    logger_.warn("Settings: Value out of range", static_cast<uint32_t>(value));
    return false;
  }
  return persist_one(*descriptor, value);
}

void SettingsManager::load_one(const settings::Descriptor &descriptor) {
  char path[MAX_PATH_LENGTH];
  if (!build_path(descriptor, path)) {
    return;
  }

  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    return; // Missing file means the default applies.
  }

  uint8_t value = 0;
  const size_t bytes_read = fread(&value, 1, 1, file);
  fclose(file);

  if (bytes_read != 1 || !settings_.set(descriptor.id, value)) {
    logger_.warn("Settings: Ignoring invalid stored value for");
    logger_.warn(descriptor.name);
  }
}

bool SettingsManager::persist_one(const settings::Descriptor &descriptor,
                                  uint8_t value) {
  char path[MAX_PATH_LENGTH];
  if (!build_path(descriptor, path)) {
    return false;
  }

  FILE *file = fopen(path, "wb");
  if (file == nullptr) {
    logger_.error("Settings: Failed to open file for writing");
    return false;
  }

  const size_t bytes_written = fwrite(&value, 1, 1, file);
  fclose(file);

  if (bytes_written != 1) {
    logger_.error("Settings: Failed to write value");
    return false;
  }
  return true;
}

} // namespace drum
