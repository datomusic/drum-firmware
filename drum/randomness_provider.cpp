#include "randomness_provider.h"
#include <algorithm>
#include <cstdlib>

namespace drum {

size_t RandomnessProvider::calculate_offset(size_t num_steps) const {
  if (num_steps == 0) {
    return 0;
  }

  return rand() % num_steps;
}

etl::array<size_t, 3>
RandomnessProvider::generate_repeat_offsets(size_t num_steps) const {
  etl::array<size_t, 3> offsets{};

  if (num_steps == 0) {
    return offsets;
  }

  for (size_t i = 0; i < 3; ++i) {
    offsets[i] = rand() % num_steps;
  }

  return offsets;
}

} // namespace drum
