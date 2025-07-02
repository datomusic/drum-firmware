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
 * @brief Holds configuration for a single sample, parsed from config.json.
 */
struct SampleConfig {
  uint8_t slot;
  uint8_t note;
  uint8_t track;
  uint32_t color;
};

/**
 * @brief Parses and provides access to system-wide configuration from config.json.
 *
 * This class is responsible for reading the main JSON configuration file,
 * parsing it with jsmn, and populating internal data structures that other
 * components can query.
 */
class ConfigurationManager {
public:
  static constexpr const char *CONFIG_PATH = "/kit.bin";
  static constexpr size_t MAX_SAMPLES = 32;


  explicit ConfigurationManager(musin::Logger &logger);

  /**
   * @brief Loads and parses the configuration file.
   *
   * @return true on successful parsing, false otherwise.
   */
  bool load();

  /**
   * @brief Gets the parsed sample configuration.
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

