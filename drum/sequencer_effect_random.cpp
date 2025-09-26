#include "sequencer_effect_random.h"
#include "config.h"
#include "pico/time.h"
#include <algorithm>
#include <cstdlib>

namespace drum {

SequencerEffectRandom::SequencerEffectRandom() {
  srand(time_us_32());

  for (size_t track_idx = 0; track_idx < MAX_TRACKS; ++track_idx) {
    current_offset_index_per_track_[track_idx] = 0;
    highlighted_random_steps_[track_idx] = 0;
  }
}

SequencerEffectRandom::RandomizedStep
SequencerEffectRandom::calculate_randomized_step(
    size_t base_step_index, size_t track_idx, size_t num_steps,
    bool repeat_active, [[maybe_unused]] uint64_t transport_step) const {
  RandomizedStep result{base_step_index, false};

  if (track_idx >= MAX_TRACKS || num_steps == 0) {
    return result;
  }

  if (random_offset_mode_active_) {
    size_t offset = 0;

    if (repeat_active) {
      offset =
          random_offsets_per_track_[track_idx]
                                   [current_offset_index_per_track_[track_idx]];
    } else {
      offset = calculate_offset(num_steps);
    }

    result.effective_step_index = (base_step_index + offset) % num_steps;
  }

  return result;
}

void SequencerEffectRandom::set_random_intensity(float intensity) {
  intensity = std::clamp(intensity, 0.0f, 1.0f);

  if (intensity < 0.2f) {
    enable_offset_mode(false);
    return;
  }

  if (!random_offset_mode_active_) {
    enable_offset_mode(true);
  }
}

void SequencerEffectRandom::enable_probability_mode(bool enabled) {
  random_probability_active_ = enabled;
}

bool SequencerEffectRandom::is_probability_mode_enabled() const {
  return random_probability_active_;
}

void SequencerEffectRandom::enable_offset_mode(bool enabled) {
  random_offset_mode_active_ = enabled;

  if (!enabled) {
    random_probability_active_ = false;
    for (size_t track_idx = 0; track_idx < MAX_TRACKS; ++track_idx) {
      current_offset_index_per_track_[track_idx] = 0;
    }
  }
}

bool SequencerEffectRandom::is_offset_mode_enabled() const {
  return random_offset_mode_active_;
}

void SequencerEffectRandom::regenerate_offsets(size_t num_steps,
                                               size_t num_tracks) {
  if (!random_offset_mode_active_) {
    return;
  }

  offset_generation_counter_++;

  const size_t tracks_to_generate = std::min(num_tracks, MAX_TRACKS);
  for (size_t track_idx = 0; track_idx < tracks_to_generate; ++track_idx) {
    random_offsets_per_track_[track_idx] = generate_repeat_offsets(num_steps);
    current_offset_index_per_track_[track_idx] = 0;
  }
}

void SequencerEffectRandom::advance_offset_indices(size_t num_tracks,
                                                   uint32_t repeat_length) {
  if (random_offset_mode_active_ &&
      repeat_length == config::analog_controls::REPEAT_LENGTH_MODE_1) {
    const size_t tracks_to_advance = std::min(num_tracks, MAX_TRACKS);
    for (size_t track_idx = 0; track_idx < tracks_to_advance; ++track_idx) {
      current_offset_index_per_track_[track_idx] =
          (current_offset_index_per_track_[track_idx] + 1) % 3;
    }
  }
}

std::optional<size_t>
SequencerEffectRandom::get_highlighted_step_for_track(size_t track_idx) const {
  if (track_idx < MAX_TRACKS && random_steps_highlighted_) {
    return highlighted_random_steps_[track_idx];
  }
  return saved_current_step_;
}

bool SequencerEffectRandom::are_steps_highlighted() const {
  return random_steps_highlighted_;
}

void SequencerEffectRandom::trigger_step_highlighting(size_t num_steps,
                                                      size_t num_tracks) {
  const size_t tracks_to_highlight = std::min(num_tracks, MAX_TRACKS);
  for (size_t track_idx = 0; track_idx < tracks_to_highlight; ++track_idx) {
    uint32_t random_value = rand();
    size_t random_step_index = random_value % num_steps;
    highlighted_random_steps_[track_idx] = random_step_index;
  }
}

void SequencerEffectRandom::start_step_highlighting() {
  random_steps_highlighted_ = true;
}

void SequencerEffectRandom::stop_step_highlighting() {
  random_steps_highlighted_ = false;
}

size_t SequencerEffectRandom::calculate_offset(size_t num_steps) {
  if (num_steps == 0) {
    return 0;
  }

  return rand() % num_steps;
}

etl::array<size_t, SequencerEffectRandom::MAX_OFFSETS_PER_TRACK>
SequencerEffectRandom::generate_repeat_offsets(size_t num_steps) {
  etl::array<size_t, MAX_OFFSETS_PER_TRACK> offsets{};

  if (num_steps == 0) {
    return offsets;
  }

  for (size_t i = 0; i < MAX_OFFSETS_PER_TRACK; ++i) {
    offsets[i] = rand() % num_steps;
  }

  return offsets;
}

} // namespace drum
