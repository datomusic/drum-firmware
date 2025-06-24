#ifndef DRUM_SAMPLE_REPOSITORY_H_
#define DRUM_SAMPLE_REPOSITORY_H_

#include "etl/array.h"
#include "etl/optional.h"
#include "etl/string.h"
#include "etl/string_view.h"

namespace drum {

/**
 * @brief Manages sample metadata, mapping slot indices to file paths.
 *
 * This class is responsible for loading a manifest file from the filesystem
 * that defines which sample file corresponds to each of the 32 sample slots.
 * It provides a central point for the AudioEngine to query for sample paths.
 */
class SampleRepository {
public:
  static constexpr size_t MAX_SAMPLES = 32;
  static constexpr size_t MAX_PATH_LENGTH = 64;
  static constexpr const char *MANIFEST_PATH = "/samples.txt";

  SampleRepository() = default;

  /**
   * @brief Populates sample paths from a configuration object.
   *
   * Clears existing paths and loads new ones based on the provided sample
   * configurations.
   *
   * @param sample_configs A vector of SampleConfig structs.
   */
  void load_from_config(const etl::ivector<struct SampleConfig> &sample_configs);

  /**
   * @brief Retrieves the file path for a given sample index.
   *
   * @param index The sample slot index (0 to MAX_SAMPLES - 1).
   * @return An optional containing a string_view to the path if it exists,
   *         or an empty optional if no sample is mapped to that index.
   */
  etl::optional<etl::string_view> get_path(size_t index) const;

private:
  // Using etl::string with a fixed capacity for embedded systems.
  using PathString = etl::string<MAX_PATH_LENGTH>;

  // An array of optional paths. If an index is empty, no sample is assigned.
  etl::array<etl::optional<PathString>, MAX_SAMPLES> sample_paths_;
};

} // namespace drum

#endif // DRUM_SAMPLE_REPOSITORY_H_
