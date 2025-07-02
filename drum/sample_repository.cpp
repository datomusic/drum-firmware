#include "drum/sample_repository.h"
#include "etl/format_spec.h"
#include "etl/to_string.h"

namespace drum {

SampleRepository::SampleRepository(musin::Logger &logger) : logger_(logger) {
}

etl::optional<etl::string_view> SampleRepository::get_path(size_t index) const {
  if (index >= MAX_SAMPLES) {
    return etl::nullopt;
  }

  // Format the path string: /<slot_number>.pcm, e.g., /05.pcm
  generated_path_.clear();
  generated_path_.append("/");

  etl::format_spec format;
  format.width(2).fill('0');

  etl::to_string(static_cast<uint16_t>(index), generated_path_, /*append=*/true, format);

  generated_path_.append(".pcm");

  return etl::string_view(generated_path_);
}

} // namespace drum
