#ifndef DRUM_RANDOMNESS_PROVIDER_H
#define DRUM_RANDOMNESS_PROVIDER_H

#include "etl/array.h"
#include <cstddef>
#include <cstdint>

namespace drum {

class RandomnessProvider {
public:
  RandomnessProvider() = default;

  size_t calculate_offset(size_t base_step, size_t track_idx,
                          float randomness_level, size_t num_steps,
                          std::uint64_t global_step_counter) const;

  etl::array<size_t, 3> generate_repeat_offsets(size_t track_idx,
                                                size_t num_steps,
                                                float randomness_level) const;

  etl::array<size_t, 3>
  generate_repeat_offsets_with_seed(size_t track_idx, size_t num_steps,
                                    float randomness_level,
                                    uint32_t seed_modifier) const;

  bool should_flip_step_probability(size_t base_step, size_t track_idx,
                                    float probability = 0.5f) const;

private:
  uint32_t generate_seed(size_t base_step, size_t track_idx, float level,
                         std::uint64_t extra_entropy = 0) const;
  size_t max_offset_for_level(float randomness_level, size_t num_steps) const;
};

} // namespace drum

#endif // DRUM_RANDOMNESS_PROVIDER_H
