#include "randomness_provider.h"
#include <algorithm>
#include <cstdlib>

namespace drum {

size_t RandomnessProvider::calculate_offset(
    [[maybe_unused]] size_t base_step, [[maybe_unused]] size_t track_idx,
    size_t num_steps,
    [[maybe_unused]] std::uint64_t global_step_counter) const {
  if (num_steps == 0) {
    return 0;
  }

  return rand() % num_steps;
}

etl::array<size_t, 3>
RandomnessProvider::generate_repeat_offsets([[maybe_unused]] size_t track_idx,
                                            size_t num_steps) const {
  etl::array<size_t, 3> offsets{};

  if (num_steps == 0) {
    return offsets;
  }

  for (size_t i = 0; i < 3; ++i) {
    offsets[i] = rand() % num_steps;
  }

  return offsets;
}

etl::array<size_t, 3> RandomnessProvider::generate_repeat_offsets_with_seed(
    [[maybe_unused]] size_t track_idx, size_t num_steps,
    [[maybe_unused]] uint32_t seed_modifier) const {
  etl::array<size_t, 3> offsets{};

  if (num_steps == 0) {
    return offsets;
  }

  for (size_t i = 0; i < 3; ++i) {
    offsets[i] = rand() % num_steps;
  }

  return offsets;
}

bool RandomnessProvider::should_flip_step_probability(
    [[maybe_unused]] size_t base_step, [[maybe_unused]] size_t track_idx,
    float probability) const {
  return (rand() % 100) < static_cast<int>(probability * 100.0f);
}

} // namespace drum
