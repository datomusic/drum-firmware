#ifndef DRUM_SEQUENCER_EFFECT_RANDOM_H
#define DRUM_SEQUENCER_EFFECT_RANDOM_H

#include "etl/array.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace drum {

enum class RandomEffectState {
  Inactive,       // No random effects active
  OffsetActive,   // Running + light press: offset randomization only
  OffsetWithFlip, // Running + hard press: offset + probability flipping
  StepPreview     // Stopped + any press: step highlighting for preview
};

class SequencerEffectRandom {
public:
  struct RandomizedStep {
    size_t effective_step_index;
    bool probability_flip_applied;
  };

  SequencerEffectRandom();

  RandomizedStep calculate_randomized_step(size_t base_step_index,
                                           size_t track_idx, size_t num_steps,
                                           bool repeat_active,
                                           uint64_t transport_step) const;

  void set_random_intensity(float intensity);

  bool is_probability_mode_enabled() const;
  bool is_offset_mode_enabled() const;

  void regenerate_offsets(size_t num_steps, size_t num_tracks);

  void advance_offset_indices(size_t num_tracks, uint32_t repeat_length);

  std::optional<size_t> get_highlighted_step_for_track(size_t track_idx) const;
  bool are_steps_highlighted() const;
  void trigger_step_highlighting(size_t num_steps, size_t num_tracks);
  void start_step_highlighting();
  void stop_step_highlighting();

  // Centralized state management
  // Enforce state transitions via this API only.
  void request_state(RandomEffectState new_state, bool repeat_active);
  void reset_to_inactive();
  RandomEffectState get_current_state() const;

private:
  static constexpr size_t MAX_TRACKS = 4;
  static constexpr size_t MAX_OFFSETS_PER_TRACK = 3;

  static size_t calculate_offset(size_t num_steps);
  static etl::array<size_t, MAX_OFFSETS_PER_TRACK>
  generate_repeat_offsets(size_t num_steps);

  // Internal helpers
  void enable_probability_mode(bool enabled);
  void enable_offset_mode(bool enabled);

  void transition_to(RandomEffectState new_state, bool repeat_active);

  RandomEffectState current_state_{RandomEffectState::Inactive};
  bool random_offset_mode_active_{false};
  bool random_probability_active_{false};

  std::array<etl::array<size_t, MAX_OFFSETS_PER_TRACK>, MAX_TRACKS>
      random_offsets_per_track_{};
  std::array<size_t, MAX_TRACKS> current_offset_index_per_track_{};
  uint32_t offset_generation_counter_{0};

  std::array<size_t, MAX_TRACKS> highlighted_random_steps_{};
  bool random_steps_highlighted_{false};
  size_t saved_current_step_{0};
};

} // namespace drum

#endif // DRUM_SEQUENCER_EFFECT_RANDOM_H
