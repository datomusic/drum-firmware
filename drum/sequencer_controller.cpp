#include "sequencer_controller.h"
#include "config.h"
#include "events.h"
#include "pico/time.h"
#include "pizza_controls.h" // For config constants
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace drum {

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::SequencerController(
    musin::timing::TempoHandler &tempo_handler_ref)
    : /* sequencer_ is default-initialized */ current_step_counter(0), last_played_note_per_track{},
      _just_played_step_per_track{}, tempo_source(tempo_handler_ref), _running(false),
      swing_percent_(50), swing_delays_odd_steps_(false), high_res_tick_counter_(0),
      next_trigger_tick_target_(0), random_active_(false),
      random_probability_(drum::config::drumpad::RANDOM_PROBABILITY_DEFAULT),
      random_track_offsets_{}, _active_note_per_track{}, _pad_pressed_state{},
      _retrigger_mode_per_track{}, _retrigger_progress_ticks_per_track{} {

  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if (track_idx < config::track_note_ranges.size() &&
        !config::track_note_ranges[track_idx].empty()) {
      _active_note_per_track[track_idx] = config::track_note_ranges[track_idx][0];
    }
  }

  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    uint8_t initial_note = _active_note_per_track[track_idx];
    auto &track = sequencer_.get_track(track_idx);
    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      auto &step = track.get_step(step_idx);
      step.note = initial_note;
      step.velocity = drum::config::keypad::DEFAULT_STEP_VELOCITY;
    }
  }

  calculate_timing_params();
  srand(time_us_32());

  // Initialize last played step to nullopt, as no step has been played yet.
  _just_played_step_per_track.fill(std::nullopt);
  _pad_pressed_state.fill(false);
}

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::~SequencerController() {
  if (_running) {
    tempo_source.remove_observer(*this);
  }
}

template <size_t NumTracks, size_t NumSteps>
size_t SequencerController<NumTracks, NumSteps>::calculate_base_step_index() const {
  const size_t num_steps = sequencer_.get_num_steps();
  if (num_steps == 0)
    return 0;

  if (repeat_active_ && repeat_length_ > 0) {
    uint64_t steps_since_activation = current_step_counter - repeat_activation_step_counter_;
    uint64_t loop_position = steps_since_activation % repeat_length_;
    return (repeat_activation_step_index_ + loop_position) % num_steps;
  } else {
    return current_step_counter % num_steps;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::process_track_step(size_t track_idx,
                                                                  size_t step_index_to_play) {
  const size_t num_steps = sequencer_.get_num_steps();
  uint8_t track_index_u8 = static_cast<uint8_t>(track_idx);

  // Emit Note Off event if a note was previously playing on this track
  if (last_played_note_per_track[track_idx].has_value()) {
    drum::Events::NoteEvent note_off_event{.track_index = track_index_u8,
                                           .note = last_played_note_per_track[track_idx].value(),
                                           .velocity = 0};
    this->notify_observers(note_off_event);
    last_played_note_per_track[track_idx] = std::nullopt;
  }

  const int effective_step_with_fixed_offset = static_cast<int>(step_index_to_play);
  const size_t wrapped_step =
      (num_steps > 0)
          ? ((effective_step_with_fixed_offset % static_cast<int>(num_steps) + num_steps) %
             num_steps)
          : 0;

  const musin::timing::Step &step = sequencer_.get_track(track_idx).get_step(wrapped_step);
  bool actually_enabled = step.enabled;

  if (random_active_) {
    // Only apply probability flip if random is active
    const bool flip_step = (rand() % 100) <= random_probability_;
    actually_enabled = flip_step ? !step.enabled : step.enabled;
  }

  if (actually_enabled && step.note.has_value() && step.velocity.has_value() &&
      step.velocity.value() > 0) {
    drum::Events::NoteEvent note_on_event{.track_index = track_index_u8,
                                          .note = step.note.value(),
                                          .velocity = step.velocity.value()};
    this->notify_observers(note_on_event);
    last_played_note_per_track[track_idx] = step.note.value();
  }
}

template <size_t NumTracks, size_t NumSteps>
uint32_t SequencerController<NumTracks, NumSteps>::calculate_next_trigger_interval() const {
  uint32_t total_ticks_for_two_steps = 2 * high_res_ticks_per_step_;
  if (total_ticks_for_two_steps == 0)
    return 1;

  uint32_t duration1 = (total_ticks_for_two_steps * swing_percent_) / 100;
  duration1 = std::max(uint32_t{1}, duration1);

  uint32_t duration2 = total_ticks_for_two_steps - duration1;
  if (duration1 >= total_ticks_for_two_steps) {
    duration2 = 1;
    duration1 = total_ticks_for_two_steps > 0 ? total_ticks_for_two_steps - 1 : 0;
  } else {
    duration2 = std::max(uint32_t{1}, duration2);
  }

  while (duration1 + duration2 > total_ticks_for_two_steps && total_ticks_for_two_steps > 0) {
    if (duration1 > 1)
      duration1--;
    else if (duration2 > 1)
      duration2--;
    else
      break;
  }

  bool current_step_is_odd = (current_step_counter % 2) != 0;
  uint32_t interval;

  if (swing_delays_odd_steps_) {
    interval = current_step_is_odd ? duration1 : duration2;
  } else {
    interval = current_step_is_odd ? duration2 : duration1;
  }

  return std::max(uint32_t{1}, interval);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::calculate_timing_params() {
  if constexpr (SEQUENCER_RESOLUTION > 0) {
    uint8_t steps_per_eight = SEQUENCER_RESOLUTION / 8;
    if (steps_per_eight > 0) {
      high_res_ticks_per_step_ = CLOCK_PPQN / steps_per_eight;
    } else {
      high_res_ticks_per_step_ = CLOCK_PPQN;
    }
  } else {
    high_res_ticks_per_step_ = 24;
  }
  high_res_ticks_per_step_ = std::max(uint32_t{1}, high_res_ticks_per_step_);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_percent(uint8_t percent) {
  swing_percent_ = std::clamp(percent, static_cast<uint8_t>(50), static_cast<uint8_t>(67));
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_target(bool delay_odd) {
  swing_delays_odd_steps_ = delay_odd;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::reset() {
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      drum::Events::NoteEvent note_off_event{.track_index = static_cast<uint8_t>(track_idx),
                                             .note = last_played_note_per_track[track_idx].value(),
                                             .velocity = 0};
      this->notify_observers(note_off_event);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  current_step_counter = 0;
  high_res_tick_counter_ = 0;

  _just_played_step_per_track.fill(std::nullopt);

  deactivate_repeat();
  deactivate_random();
  for (size_t i = 0; i < NumTracks; ++i) {
    deactivate_play_on_every_step(static_cast<uint8_t>(i));
  }

  uint32_t first_interval = calculate_next_trigger_interval();
  next_trigger_tick_target_ = first_interval;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::start() {
  if (_running) {
    return;
  }

  _just_played_step_per_track.fill(std::nullopt);

  tempo_source.add_observer(*this);
  tempo_source.set_playback_state(musin::timing::PlaybackState::PLAYING);

  _running = true;
}

template <size_t NumTracks, size_t NumSteps> void SequencerController<NumTracks, NumSteps>::stop() {
  if (!_running) {
    return;
  }
  tempo_source.set_playback_state(musin::timing::PlaybackState::STOPPED);
  tempo_source.remove_observer(*this);
  _running = false;

  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      drum::Events::NoteEvent note_off_event{.track_index = static_cast<uint8_t>(track_idx),
                                             .note = last_played_note_per_track[track_idx].value(),
                                             .velocity = 0};
      this->notify_observers(note_off_event);
    }
  }
  for (size_t i = 0; i < NumTracks; ++i) {
    deactivate_play_on_every_step(static_cast<uint8_t>(i));
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    [[maybe_unused]] musin::timing::TempoEvent event) {
  if (!_running) {
    return;
  }

  high_res_tick_counter_++;

  // Process per-tick retrigger logic (e.g., for double mode's mid-step note)
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if (_retrigger_mode_per_track[track_idx] > 0) {
      _retrigger_progress_ticks_per_track[track_idx]++;

      if (_retrigger_mode_per_track[track_idx] == 2 && // Double mode
          high_res_ticks_per_step_ >=
              drum::config::main_controls::RETRIGGER_DIVISOR_FOR_DOUBLE_MODE &&
          _retrigger_progress_ticks_per_track[track_idx] ==
              (high_res_ticks_per_step_ /
               drum::config::main_controls::RETRIGGER_DIVISOR_FOR_DOUBLE_MODE)) {
        uint8_t note_to_play = get_active_note_for_track(static_cast<uint8_t>(track_idx));
        trigger_note_on(static_cast<uint8_t>(track_idx), note_to_play,
                        drum::config::drumpad::RETRIGGER_VELOCITY);
      }
    }
  }

  if (high_res_tick_counter_ >= next_trigger_tick_target_) {
    _just_played_step_per_track.fill(std::nullopt);

    size_t base_step_index = calculate_base_step_index();

    size_t num_tracks = sequencer_.get_num_tracks();
    size_t num_steps = sequencer_.get_num_steps();

    for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
      size_t step_index_to_play_for_track = base_step_index;

      if (random_active_ && num_steps > 0) {
        int max_offset = num_steps / 2;
        random_track_offsets_[track_idx] = (rand() % (max_offset * 2 + 1)) - max_offset;
        step_index_to_play_for_track =
            (base_step_index + random_track_offsets_[track_idx] + num_steps) % num_steps;
      }
      _just_played_step_per_track[track_idx] = step_index_to_play_for_track;
      process_track_step(track_idx, step_index_to_play_for_track);

      // Handle the first retrigger note for the main step event
      if (_retrigger_mode_per_track[track_idx] > 0) {
        uint8_t note_to_play = get_active_note_for_track(static_cast<uint8_t>(track_idx));
        trigger_note_on(static_cast<uint8_t>(track_idx), note_to_play,
                        drum::config::drumpad::RETRIGGER_VELOCITY);
      }
      // Reset retrigger progress for this track as a new main step has occurred
      _retrigger_progress_ticks_per_track[track_idx] = 0;
    }

    uint32_t interval_to_next_trigger = calculate_next_trigger_interval();
    next_trigger_tick_target_ += interval_to_next_trigger;

    current_step_counter++;
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t SequencerController<NumTracks, NumSteps>::get_current_step() const noexcept {
  const size_t num_steps = sequencer_.get_num_steps();
  if (num_steps == 0)
    return 0;
  return current_step_counter % num_steps;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] std::optional<size_t>
SequencerController<NumTracks, NumSteps>::get_last_played_step_for_track(size_t track_idx) const {
  if ((track_idx < NumTracks)) {
    return _just_played_step_per_track[track_idx];
  }
  return std::nullopt;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_running() const {
  return _running;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_repeat(uint32_t length) {
  if (_running && !repeat_active_) {
    repeat_active_ = true;
    repeat_length_ = std::max(uint32_t{1}, length);
    const size_t num_steps = sequencer_.get_num_steps();
    repeat_activation_step_index_ = (num_steps > 0) ? (current_step_counter % num_steps) : 0;
    repeat_activation_step_counter_ = current_step_counter;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_repeat() {
  if (repeat_active_) {
    repeat_active_ = false;
    repeat_length_ = 0;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_repeat_length(uint32_t length) {
  if (repeat_active_) {
    uint32_t new_length = std::max(uint32_t{1}, length);
    if (new_length != repeat_length_) {
      repeat_length_ = new_length;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_repeat_active() const {
  return repeat_active_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_random() {
  if (_running && !random_active_) {
    random_active_ = true;
    random_track_offsets_ = {};
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_random() {
  if (random_active_) {
    random_active_ = false;
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_random_active() const {
  return random_active_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_random_probability(uint8_t percent) {
  random_probability_ = std::clamp(percent, static_cast<uint8_t>(0), static_cast<uint8_t>(100));
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_intended_repeat_state(
    std::optional<uint32_t> intended_length) {
  bool should_be_active = intended_length.has_value();
  bool was_active = is_repeat_active();

  if (should_be_active && !was_active) {
    activate_repeat(intended_length.value());
  } else if (!should_be_active && was_active) {
    deactivate_repeat();
  } else if (should_be_active && was_active) {
    set_repeat_length(intended_length.value());
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t
SequencerController<NumTracks, NumSteps>::get_ticks_per_musical_step() const noexcept {
  return high_res_ticks_per_step_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::toggle() {
  if (is_running()) {
    stop();
  } else {
    start();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  if (event.is_active) {
    stop();
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::trigger_note_on(uint8_t track_index, uint8_t note,
                                                               uint8_t velocity) {
  // Ensure any previously playing note on this track is turned off first
  if (last_played_note_per_track[track_index].has_value()) {
    if (last_played_note_per_track[track_index].value() !=
        note) { // Only send note off if it's a different note
      drum::Events::NoteEvent note_off_event{.track_index = track_index,
                                             .note =
                                                 last_played_note_per_track[track_index].value(),
                                             .velocity = 0};
      this->notify_observers(note_off_event);
    }
  }

  drum::Events::NoteEvent note_on_event{
      .track_index = track_index, .note = note, .velocity = velocity};
  this->notify_observers(note_on_event);
  last_played_note_per_track[track_index] = note;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::trigger_note_off(uint8_t track_index, uint8_t note) {
  // Only send note off if this note was the one playing
  if (last_played_note_per_track[track_index].has_value() &&
      last_played_note_per_track[track_index].value() == note) {
    drum::Events::NoteEvent note_off_event{.track_index = track_index, .note = note, .velocity = 0};
    this->notify_observers(note_off_event);
    last_played_note_per_track[track_index] = std::nullopt;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_active_note_for_track(uint8_t track_index,
                                                                         uint8_t note) {
  if (track_index < NumTracks) {
    _active_note_per_track[track_index] = note;
  }
  // else: track_index is out of bounds, do nothing or log an error.
  // For now, we silently ignore out-of-bounds access to prevent crashes.
}

template <size_t NumTracks, size_t NumSteps>
uint8_t
SequencerController<NumTracks, NumSteps>::get_active_note_for_track(uint8_t track_index) const {
  if (track_index < NumTracks) {
    return _active_note_per_track[track_index];
  }
  // track_index is out of bounds. Return a default/safe value.
  // 0 is a common default for MIDI notes, though often unassigned.
  return 0;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_pad_pressed_state(uint8_t track_index,
                                                                     bool is_pressed) {
  if (track_index < NumTracks) {
    _pad_pressed_state[track_index] = is_pressed;
  }
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::is_pad_pressed(uint8_t track_index) const {
  if (track_index < NumTracks) {
    return _pad_pressed_state[track_index];
  }
  return false;
}

template <size_t NumTracks, size_t NumSteps>
uint8_t
SequencerController<NumTracks, NumSteps>::get_retrigger_mode_for_track(uint8_t track_index) const {
  if (track_index < NumTracks) {
    return _retrigger_mode_per_track[track_index];
  }
  return 0;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_play_on_every_step(uint8_t track_index,
                                                                           uint8_t mode) {
  if (track_index < NumTracks && (mode == 1 || mode == 2)) {
    _retrigger_mode_per_track[track_index] = mode;
    _retrigger_progress_ticks_per_track[track_index] = 0;
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_play_on_every_step(uint8_t track_index) {
  if (track_index < NumTracks) {
    _retrigger_mode_per_track[track_index] = 0;
    _retrigger_progress_ticks_per_track[track_index] = 0;
  }
}

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerController<4, 8>;

} // namespace drum
