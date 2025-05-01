#include "sequencer_controller.h"
#include "midi.h"
#include <algorithm>
#include <cstdio>

namespace StepSequencer {

template <size_t NumTracks, size_t NumSteps>
SequencerController<NumTracks, NumSteps>::SequencerController(
    StepSequencer::Sequencer<NumTracks, NumSteps> &sequencer_ref,
    etl::observable<etl::observer<Tempo::SequencerTickEvent>, 2> &tempo_source_ref)
    : sequencer(sequencer_ref), current_step_counter(0), last_played_note_per_track{},
      last_played_step_index_(0), tempo_source(tempo_source_ref), state_(State::Stopped),
      swing_percent_(50), swing_delays_odd_steps_(false), high_res_tick_counter_(0),
      next_trigger_tick_target_(0) {
  calculate_timing_params();
  printf("SequencerController: Initialized. Ticks/Step: %lu\n", high_res_ticks_per_step_);
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
  high_res_ticks_per_step_ = std::max(static_cast<uint32_t>(1u), high_res_ticks_per_step_);
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
      uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);
      send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  current_step_counter = 0;
  high_res_tick_counter_ = 0;
  last_played_step_index_ = 0;

  uint32_t total_ticks_for_two_steps = 2 * high_res_ticks_per_step_;
  uint32_t duration1 = (total_ticks_for_two_steps * swing_percent_) / 100;
  uint32_t duration2 = total_ticks_for_two_steps - duration1;

  duration1 = std::max(static_cast<uint32_t>(1u), duration1);
  if (duration1 >= total_ticks_for_two_steps) {
    duration2 = 0;
    duration1 = total_ticks_for_two_steps;
  } else {
    duration2 = total_ticks_for_two_steps - duration1;
  }
  duration2 = std::max(static_cast<uint32_t>(1u), duration2);

  if (duration1 + duration2 > total_ticks_for_two_steps && total_ticks_for_two_steps > 0) {
    if (duration1 > 1)
      duration1--;
    else if (duration2 > 1)
      duration2--;
  }

  uint32_t first_step_duration;
  bool step0_is_odd = (0 % 2) != 0;

  if (swing_delays_odd_steps_) {
    first_step_duration = step0_is_odd ? duration1 : duration2;
  } else {
    first_step_duration = step0_is_odd ? duration2 : duration1;
  }

  next_trigger_tick_target_ = std::max(1ul, static_cast<unsigned long>(first_step_duration));
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

  // Send note-offs for all active notes
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);
      send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  printf("SequencerController: Stopped\n");
  return true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::update_swing_durations() {
  const uint32_t total_ticks = 2 * high_res_ticks_per_step_;
  swing_duration1_ = (total_ticks * swing_percent_) / 100;
  swing_duration2_ = total_ticks - swing_duration1_;
  swing_duration1_ = std::max(static_cast<uint32_t>(1u), swing_duration1_);
  swing_duration2_ = std::max(static_cast<uint32_t>(1u), swing_duration2_);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::notification(
    [[maybe_unused]] Tempo::SequencerTickEvent event) {
  if (state_ != State::Running)
    return;

  high_res_tick_counter_++;

  if (high_res_tick_counter_ >= next_trigger_tick_target_) {

    const size_t num_steps = sequencer.get_num_steps();
    size_t step_index_to_play;
    if (repeat_active_ && repeat_length_ > 0 && num_steps > 0) {
      uint64_t steps_since_activation = current_step_counter - repeat_activation_step_counter_;
      uint64_t loop_position = steps_since_activation % repeat_length_;
      step_index_to_play = (repeat_activation_step_index_ + loop_position) % num_steps;
    } else {
      step_index_to_play = (num_steps > 0) ? (current_step_counter % num_steps) : 0;
    }

    last_played_step_index_ = step_index_to_play;

    size_t num_tracks = sequencer.get_num_tracks();
    for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
      uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);

      if (last_played_note_per_track[track_idx].has_value()) {
        send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0);
        last_played_note_per_track[track_idx] = std::nullopt;
      }

      const int effective_step = static_cast<int>(step_index_to_play) + track_offsets_[track_idx];
      const size_t wrapped_step =
          (num_steps > 0) ? ((effective_step % static_cast<int>(num_steps) + num_steps) % num_steps)
                          : 0;

      const Step &step = sequencer.get_track(track_idx).get_step(wrapped_step);

      if (step.enabled && step.note.has_value() && step.velocity.has_value() &&
          step.velocity.value() > 0) {
        send_midi_note(midi_channel, step.note.value(), step.velocity.value());
        last_played_note_per_track[track_idx] = step.note.value();
      }
    }

    uint32_t total_ticks_for_two_steps = 2 * high_res_ticks_per_step_;
    uint32_t duration1 = (total_ticks_for_two_steps * swing_percent_) / 100;
    uint32_t duration2 = total_ticks_for_two_steps - duration1;

    duration1 = std::max(static_cast<uint32_t>(1u), duration1);
    if (duration1 >= total_ticks_for_two_steps) {
      duration2 = 0;
      duration1 = total_ticks_for_two_steps;
    } else {
      duration2 = total_ticks_for_two_steps - duration1;
    }
    duration2 = std::max(static_cast<uint32_t>(1u), duration2);

    if (duration1 + duration2 > total_ticks_for_two_steps && total_ticks_for_two_steps > 0) {
      if (duration1 > 1)
        duration1--;
      else if (duration2 > 1)
        duration2--;
    }

    uint32_t interval_to_next_trigger;
    bool current_step_was_odd = (current_step_counter % 2) != 0;

    if (swing_delays_odd_steps_) {
      interval_to_next_trigger = current_step_was_odd ? duration1 : duration2;
    } else {
      interval_to_next_trigger = current_step_was_odd ? duration2 : duration1;
    }

    next_trigger_tick_target_ += std::max(static_cast<uint32_t>(1u), interval_to_next_trigger);

    current_step_counter++;
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
[[nodiscard]] uint32_t
SequencerController<NumTracks, NumSteps>::get_last_played_step_index() const noexcept {
  return last_played_step_index_;
}

template <size_t NumTracks, size_t NumSteps>
[[nodiscard]] bool SequencerController<NumTracks, NumSteps>::is_running() const {
  return state_ == State::Running;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerController<NumTracks, NumSteps>::activate_repeat(uint32_t length) {
  if (state_ == State::Running && !repeat_active_) {
    repeat_active_ = true;
    repeat_length_ = std::max(static_cast<uint32_t>(1u), length);
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
    uint32_t new_length = std::max(static_cast<uint32_t>(1u), length);
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

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerController<4, 8>;

} // namespace StepSequencer
