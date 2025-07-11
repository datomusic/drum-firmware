#ifndef DRUM_SAMPLE_REPOSITORY_H_
#define DRUM_SAMPLE_REPOSITORY_H_

#include "etl/array.h"
#include "etl/optional.h"
#include "etl/string.h"
#include "etl/string_view.h"
#include "etl/vector.h"

#include "musin/hal/logger.h"

namespace drum {

/**
 * @brief Generates file paths for sample slots based on a fixed naming
 * convention.
 *
 * This class provides a central point for the AudioEngine to query for sample
 * paths. It dynamically constructs the path for a given slot index, e.g., slot
 * 0 becomes "/00.pcm".
 */
class SampleRepository {
public:
  static constexpr size_t MAX_SAMPLES = 32;
  static constexpr size_t MAX_PATH_LENGTH = 16; // "/NN.pcm" is small

  explicit SampleRepository(musin::Logger &logger);

  /**
   * @brief Retrieves the file path for a given sample index.
   *
   * @param index The sample slot index (0 to MAX_SAMPLES - 1).
   * @return An optional containing a string_view to the path if it exists,
   *         or an empty optional if no sample is mapped to that index.
   */
  etl::optional<etl::string_view> get_path(size_t index) const;

private:
  musin::Logger &logger_;

  // Using etl::string with a fixed capacity for embedded systems.
  using PathString = etl::string<MAX_PATH_LENGTH>;

  // A mutable string to hold the dynamically generated path.
  mutable PathString generated_path_;
};

} // namespace drum

#endif // DRUM_SAMPLE_REPOSITORY_H_
