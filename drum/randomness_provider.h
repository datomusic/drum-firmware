#ifndef DRUM_RANDOMNESS_PROVIDER_H
#define DRUM_RANDOMNESS_PROVIDER_H

#include "etl/array.h"
#include <cstddef>
#include <cstdint>

namespace drum {

class RandomnessProvider {
public:
  RandomnessProvider() = default;

  size_t calculate_offset(size_t num_steps) const;

  etl::array<size_t, 3> generate_repeat_offsets(size_t num_steps) const;

  etl::array<size_t, 3>
  generate_repeat_offsets_with_seed(size_t num_steps) const;

  bool should_flip_step_probability(float probability = 0.5f) const;

private:
  // All methods now use simple rand() calls
};

} // namespace drum

#endif // DRUM_RANDOMNESS_PROVIDER_H
