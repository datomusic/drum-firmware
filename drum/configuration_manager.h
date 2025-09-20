#ifndef DRUM_CONFIGURATION_MANAGER_H_
#define DRUM_CONFIGURATION_MANAGER_H_

#include "config.h"
#include "etl/string.h"
#include "etl/string_view.h"
#include "etl/vector.h"
#include <cstdint>

#include "drum/sample_repository.h"
#include "musin/hal/logger.h"

namespace drum {

/**
 * @brief Holds configuration for a single sample slot.
 * This is the runtime representation of the data loaded from a binary kit file.
 */
struct SampleConfig {
  uint8_t slot;
  uint8_t note;
  uint8_t track;
  uint32_t color;
};

/**
 * @brief Manages loading and accessing sample kit configurations.
 *
 * This class reads a binary kit file (`/kit.bin`) from the filesystem, which
 * defines the properties for each of the 32 sample slots (MIDI note, color,
 * track assignment). If `/kit.bin` is not found, it falls back to loading a
 * default, compile-time generated `/factory_kit.bin`.
 *
 * The binary kit data originates from `factory_kit.cpp`, which is converted
 * to a binary file at build time via a custom CMake command using `objcopy`.
 */
class ConfigurationManager {
public:
  static constexpr const char *CONFIG_PATH = "/kit.bin";
  static constexpr size_t MAX_SAMPLES = 128;

  explicit ConfigurationManager(musin::Logger &logger);

  /**
   * @brief Loads and parses the kit configuration file.
   *
   * @return true on successful loading, false otherwise.
   */
  bool load();

  /**
   * @brief Gets the loaded sample configuration.
   *
   * @return A const reference to the vector of sample configurations.
   */
  const etl::ivector<SampleConfig> &get_sample_configs() const;

private:
  musin::Logger &logger_;
  bool load_factory_kit();

  etl::vector<SampleConfig, MAX_SAMPLES> sample_configs_;
};

} // namespace drum

#endif // DRUM_CONFIGURATION_MANAGER_H_
