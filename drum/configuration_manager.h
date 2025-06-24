#ifndef DRUM_CONFIGURATION_MANAGER_H_
#define DRUM_CONFIGURATION_MANAGER_H_

#include "etl/string.h"
#include "etl/vector.h"
#include <cstdint>

#include "drum/sample_repository.h"

// Forward declaration of jsmn token struct
struct jsmntok;

namespace drum {

/**
 * @brief Holds configuration for a single sample, parsed from config.json.
 */
struct SampleConfig {
  uint8_t slot;
  etl::string<SampleRepository::MAX_PATH_LENGTH> path;
  uint8_t note;
  uint8_t track;
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
  static constexpr const char *CONFIG_PATH = "/config.json";
  static constexpr size_t MAX_CONFIG_FILE_SIZE = 4096;
  static constexpr size_t MAX_JSON_TOKENS = 256;

  ConfigurationManager() = default;

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
  bool json_string_equals(const char *json, const jsmntok *token, const char *str);
  bool parse_samples(const char *json, jsmntok *tokens, int count);

  etl::vector<SampleConfig, SampleRepository::MAX_SAMPLES> sample_configs_;
};

} // namespace drum

#endif // DRUM_CONFIGURATION_MANAGER_H_
