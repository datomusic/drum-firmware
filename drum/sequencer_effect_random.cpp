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
  // Internal toggle; does not modify current_state_.
  random_probability_active_ = enabled;
}

bool SequencerEffectRandom::is_probability_mode_enabled() const {
  return random_probability_active_;
}

void SequencerEffectRandom::enable_offset_mode(bool enabled) {
  // Internal toggle; does not modify current_state_. Also clears probability
  // when disabling.
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
  // StepPreview is only meaningful when stopped; repeat flag irrelevant.
  request_state(RandomEffectState::StepPreview, /*repeat_active=*/false);
}

void SequencerEffectRandom::stop_step_highlighting() {
  random_steps_highlighted_ = false;
  if (current_state_ == RandomEffectState::StepPreview) {
    request_state(RandomEffectState::Inactive, /*repeat_active=*/false);
  }
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

void SequencerEffectRandom::reset_to_inactive() {
  transition_to(RandomEffectState::Inactive, /*repeat_active=*/false);
}

RandomEffectState SequencerEffectRandom::get_current_state() const {
  return current_state_;
}

void SequencerEffectRandom::request_state(RandomEffectState new_state,
                                          bool repeat_active) {
  transition_to(new_state, repeat_active);
}

void SequencerEffectRandom::transition_to(RandomEffectState new_state,
                                          bool repeat_active) {
  // Enforce REPEAT constraint: no OffsetWithFlip while repeating
  if (repeat_active && new_state == RandomEffectState::OffsetWithFlip) {
    new_state = RandomEffectState::OffsetActive;
  }

  if (current_state_ == new_state) {
    return;
  }

  // Exit current state
  switch (current_state_) {
  case RandomEffectState::OffsetActive:
  case RandomEffectState::OffsetWithFlip:
    enable_probability_mode(false);
    enable_offset_mode(false);
    break;
  case RandomEffectState::StepPreview:
    // We are already in StepPreview; ensure preview flag off
    random_steps_highlighted_ = false;
    break;
  case RandomEffectState::Inactive:
    break;
  }

  // Enter new state
  switch (new_state) {
  case RandomEffectState::OffsetActive:
    enable_offset_mode(true);
    break;
  case RandomEffectState::OffsetWithFlip:
    enable_offset_mode(true);
    enable_probability_mode(true);
    break;
  case RandomEffectState::StepPreview:
    // Preview is controlled by start/stop_step_highlighting
    break;
  case RandomEffectState::Inactive:
    // Already cleaned up in exit logic
    break;
  }

  current_state_ = new_state;
}

} // namespace drum
