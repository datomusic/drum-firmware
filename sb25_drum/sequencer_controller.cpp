#include "sequencer_controller.h"
#include "events.h"         // Added for NoteEvent
#include "pico/time.h"      // For time_us_32() for seeding rand
#include "pizza_controls.h" // Include for PizzaControls pointer type (used for drumpad fade)
// #include "sound_router.h"   // Removed
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace StepSequencer {

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::SequencerController(
    Musin::Timing::Sequencer<NumTracks, NumSteps> &sequencer_ref,
    etl::observable<etl::observer<Musin::Timing::SequencerTickEvent>, 2>
        &tempo_source_ref) // Removed sound_router_ref
    : sequencer(sequencer_ref), // Removed _sound_router init
      current_step_counter(0), last_played_note_per_track{}, _just_played_step_per_track{},
      tempo_source(tempo_source_ref), state_(State::Stopped), swing_percent_(50),
      swing_delays_odd_steps_(false), high_res_tick_counter_(0), next_trigger_tick_target_(0),
      random_active_(false), random_track_offsets_{} {
  calculate_timing_params();
  // Seed the random number generator once
  srand(time_us_32());

  // Initialize last played step to the final step index for initial highlight
  if constexpr (NumSteps > 0) {
    _just_played_step_per_track.fill(NumSteps - 1);
  } else {
    _just_played_step_per_track.fill(std::nullopt);
  }
}

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::~SequencerController() {
  if (state_ != State::Stopped) {
    tempo_source.remove_observer(*this);
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_state(State new_state) {
  if (state_ != new_state) {
    state_ = new_state;
  }
}

// --- Helper Methods ---

template <size_t NumTracks, size_t NumSteps>
size_t SequencerController<NumTracks, NumSteps>::calculate_base_step_index() const {
  const size_t num_steps = sequencer.get_num_steps();
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
  // uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1); // No longer needed here
  const size_t num_steps = sequencer.get_num_steps();

  // Send Note Off via SoundRouter if a note was previously playing on this track
  if (last_played_note_per_track[track_idx].has_value()) {
    _sound_router.trigger_sound(static_cast<uint8_t>(track_idx),
                                last_played_note_per_track[track_idx].value(), 0);
    last_played_note_per_track[track_idx] = std::nullopt;
  }

  const int effective_step_with_fixed_offset =
      static_cast<int>(step_index_to_play) + track_offsets_[track_idx];
  const size_t wrapped_step =
      (num_steps > 0)
          ? ((effective_step_with_fixed_offset % static_cast<int>(num_steps) + num_steps) %
             num_steps)
          : 0;

  const Musin::Timing::Step &step = sequencer.get_track(track_idx).get_step(wrapped_step);
  if (step.enabled && step.note.has_value() && step.velocity.has_value() &&
      step.velocity.value() > 0) {
    // Emit Note On event
    SB25::Events::NoteEvent note_on_event{.track_index = track_index_u8,
                                          .note = step.note.value(),
                                          .velocity = step.velocity.value()};
    notify_observers(note_on_event);
    last_played_note_per_track[track_idx] = step.note.value();

    // Trigger fade on the corresponding drumpad if controls pointer is set
    // This remains as it's a visual effect, not sound generation.
    if (_controls_ptr) {
      _controls_ptr->drumpad_component.trigger_fade(static_cast<uint8_t>(track_idx));
    }
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

// --- Public Methods ---

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::calculate_timing_params() {
  if constexpr (SEQUENCER_RESOLUTION > 0) {
    uint8_t steps_per_quarter = SEQUENCER_RESOLUTION / 4;
    if (steps_per_quarter > 0) {
      high_res_ticks_per_step_ = CLOCK_PPQN / steps_per_quarter;
    } else {
      high_res_ticks_per_step_ = CLOCK_PPQN;
    }
  } else {
    high_res_ticks_per_step_ = 24; // Default fallback
  }
  high_res_ticks_per_step_ = std::max(uint32_t{1}, high_res_ticks_per_step_);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_percent(uint8_t percent) {
  swing_percent_ = std::clamp(percent, static_cast<uint8_t>(50), static_cast<uint8_t>(75));
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_swing_target(bool delay_odd) {
  swing_delays_odd_steps_ = delay_odd;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::reset() {
  printf("SequencerController: Resetting.\n");
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      // Emit Note Off event
      SB25::Events::NoteEvent note_off_event{
          .track_index = static_cast<uint8_t>(track_idx),
          .note = last_played_note_per_track[track_idx].value(),
          .velocity = 0};
      notify_observers(note_off_event);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  current_step_counter = 0;
  high_res_tick_counter_ = 0;

  // Reset the 'just played' step index for each track to the *last* step
  if constexpr (NumSteps > 0) {
    _just_played_step_per_track.fill(NumSteps - 1);
  } else {
    _just_played_step_per_track.fill(std::nullopt);
  }

  // Ensure effects are reset
  deactivate_repeat();
  deactivate_random();

  uint32_t first_interval = calculate_next_trigger_interval();
  next_trigger_tick_target_ = first_interval;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerController<NumTracks, NumSteps>::start() {
  if (state_ != State::Stopped) {
    printf("SequencerController: Already running\n");
    return false;
  }
  reset();
  tempo_source.add_observer(*this);
  set_state(State::Running);
  printf("SequencerController: Started. Waiting for tick %llu\n", next_trigger_tick_target_);
  return true;
}

template <size_t NumTracks, size_t NumSteps> bool SequencerController<NumTracks, NumSteps>::stop() {
  if (state_ == State::Stopped) {
    printf("SequencerController: Already stopped\n");
    return false;
  }
  tempo_source.remove_observer(*this);
  set_state(State::Stopped);

  // Emit note-offs for all active notes
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      // Emit Note Off event
      SB25::Events::NoteEvent note_off_event{
          .track_index = static_cast<uint8_t>(track_idx),
          .note = last_played_note_per_track[track_idx].value(),
          .velocity = 0};
      notify_observers(note_off_event);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  printf("SequencerController: Stopped\n");
  return true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    [[maybe_unused]] Musin::Timing::SequencerTickEvent event) {
  if (state_ != State::Running)
    return;

  high_res_tick_counter_++;

  if (high_res_tick_counter_ >= next_trigger_tick_target_) {
    // Clear the per-track played state for this trigger cycle
    _just_played_step_per_track.fill(std::nullopt);

    // 1. Determine the base step index (where the sequencer would be without effects)
    //    This now also considers the repeat effect if active.
    size_t base_step_index = calculate_base_step_index();

    // Note: We no longer store a single 'last_played_step_index_' here.
    // Instead, we store the actual played step per track below.

    size_t num_tracks = sequencer.get_num_tracks();
    size_t num_steps = sequencer.get_num_steps();

    // 3. Process each track
    for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
      size_t step_index_to_play_for_track = base_step_index;

      // If random is active, calculate a *new* random offset for this track *this step*
      if (random_active_ && num_steps > 0) {
        // Example: offset range +/- half the steps, centered around 0
        int max_offset = num_steps / 2;
        random_track_offsets_[track_idx] = (rand() % (max_offset * 2 + 1)) - max_offset;
        step_index_to_play_for_track =
            (base_step_index + random_track_offsets_[track_idx] + num_steps) % num_steps;
      }
      // Store the actual step played for this track (for display/highlighting)
      _just_played_step_per_track[track_idx] = step_index_to_play_for_track;
      // Process the step (send MIDI etc.)
      process_track_step(track_idx, step_index_to_play_for_track);
    }

    uint32_t interval_to_next_trigger = calculate_next_trigger_interval();
    next_trigger_tick_target_ += interval_to_next_trigger;

    current_step_counter++; // Increment after processing the current step and calculating next
                            // interval
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] uint32_t SequencerController<NumTracks, NumSteps>::get_current_step() const noexcept {
  const size_t num_steps = sequencer.get_num_steps();
  if (num_steps == 0)
    return 0;
  return current_step_counter % num_steps;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] std::optional<size_t>
SequencerController<NumTracks, NumSteps>::get_last_played_step_for_track(size_t track_idx) const {
  if (track_idx < NumTracks) {
    return _just_played_step_per_track[track_idx];
  }
  return std::nullopt;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_running() const {
  return state_ == State::Running;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_repeat(uint32_t length) {
  if (state_ == State::Running && !repeat_active_) {
    repeat_active_ = true;
    repeat_length_ = std::max(uint32_t{1}, length);
    const size_t num_steps = sequencer.get_num_steps();
    repeat_activation_step_index_ = (num_steps > 0) ? (current_step_counter % num_steps) : 0;
    repeat_activation_step_counter_ = current_step_counter;
    printf("Repeat Activated: Length %lu, Start Step %lu (Abs Counter %llu)\n", repeat_length_,
           repeat_activation_step_index_, repeat_activation_step_counter_);
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_repeat() {
  if (repeat_active_) {
    repeat_active_ = false;
    repeat_length_ = 0;
    printf("Repeat Deactivated\n");
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_repeat_length(uint32_t length) {
  if (repeat_active_) {
    uint32_t new_length = std::max(uint32_t{1}, length);
    if (new_length != repeat_length_) {
      repeat_length_ = new_length;
      printf("Repeat Length Changed: New Length %lu\n", repeat_length_);
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_repeat_active() const {
  return repeat_active_;
}

// --- Random Effect Methods ---

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_random() {
  if (state_ == State::Running && !random_active_) {
    random_active_ = true;
    random_track_offsets_ = {}; // Reset offsets when activating
    printf("Random Activated\n");
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::deactivate_random() {
  if (random_active_) {
    random_active_ = false;
    printf("Random Deactivated\n");
  }
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_random_active() const {
  return random_active_;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::set_controls_ptr(PizzaControls *ptr) {
  _controls_ptr = ptr;
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
void SequencerController<NumTracks, NumSteps>::toggle() {
  if (is_running()) {
    stop();
  } else {
    start();
  }
}

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerController<4, 8>;

} // namespace StepSequencer
