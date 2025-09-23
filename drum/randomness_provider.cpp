#include "randomness_provider.h"
#include <algorithm>
#include <cstdlib>

namespace drum {

size_t
RandomnessProvider::calculate_offset(size_t base_step, size_t track_idx,
                                     float randomness_level, size_t num_steps,
                                     std::uint64_t global_step_counter) const {
  if (randomness_level <= 0.0f || num_steps == 0) {
    return 0;
  }

  uint32_t seed = generate_seed(base_step, track_idx, randomness_level,
                                global_step_counter);
  size_t max_offset = max_offset_for_level(randomness_level, num_steps);

  return seed % (max_offset + 1);
}

etl::array<size_t, 3>
RandomnessProvider::generate_repeat_offsets(size_t track_idx, size_t num_steps,
                                            float randomness_level) const {
  etl::array<size_t, 3> offsets{};

  if (num_steps == 0) {
    return offsets;
  }

  for (size_t i = 0; i < 3; ++i) {
    uint32_t seed = generate_seed(i, track_idx, randomness_level);
    size_t max_offset = max_offset_for_level(randomness_level, num_steps);
    offsets[i] = seed % (max_offset + 1);
  }

  return offsets;
}

etl::array<size_t, 3> RandomnessProvider::generate_repeat_offsets_with_seed(
    size_t track_idx, size_t num_steps, float randomness_level,
    uint32_t seed_modifier) const {
  etl::array<size_t, 3> offsets{};

  if (num_steps == 0) {
    return offsets;
  }

  for (size_t i = 0; i < 3; ++i) {
    // Include seed_modifier to make each generation unique
    uint32_t base_seed = generate_seed(i, track_idx, randomness_level);
    uint32_t final_seed = base_seed ^ (seed_modifier * 31);
    size_t max_offset = max_offset_for_level(randomness_level, num_steps);
    offsets[i] = final_seed % (max_offset + 1);
  }

  return offsets;
}

bool RandomnessProvider::should_flip_step_probability(size_t base_step,
                                                      size_t track_idx,
                                                      float probability) const {
  uint32_t seed = generate_seed(base_step, track_idx, probability);
  return (seed & 0xFF) < static_cast<uint32_t>(probability * 255.0f);
}

uint32_t RandomnessProvider::generate_seed(size_t base_step, size_t track_idx,
                                           float level,
                                           std::uint64_t extra_entropy) const {
  uint32_t seed = static_cast<uint32_t>(base_step);
  seed = seed * 31 + static_cast<uint32_t>(track_idx);
  seed = seed * 31 + static_cast<uint32_t>(level * 1000.0f);

  seed = seed * 31 + static_cast<uint32_t>(extra_entropy & 0xFFFFFFFFu);
  seed = seed * 31 + static_cast<uint32_t>((extra_entropy >> 32) & 0xFFFFFFFFu);

  seed ^= (seed >> 16);
  seed *= 0x85ebca6b;
  seed ^= (seed >> 13);
  seed *= 0xc2b2ae35;
  seed ^= (seed >> 16);

  return seed;
}

size_t RandomnessProvider::max_offset_for_level(float randomness_level,
                                                size_t num_steps) const {
  float scaled_level = std::clamp(randomness_level, 0.0f, 1.0f);
  return static_cast<size_t>(scaled_level * static_cast<float>(num_steps - 1));
}

} // namespace drum
