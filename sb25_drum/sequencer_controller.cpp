#include "sequencer_controller.h"
#include "midi.h"    // For send_midi_note
#include <algorithm> // For std::clamp, std::max
#include <cstdio> // For printf

namespace StepSequencer {

SequencerController::SequencerController(
    StepSequencer::Sequencer<4, 8> &sequencer_ref,
    etl::observable<etl::observer<Tempo::SequencerTickEvent>, 2> &tempo_source_ref)
    : sequencer(sequencer_ref), current_step_counter(0), last_played_note_per_track{},
      tempo_source(tempo_source_ref), running(false), swing_percent_(50),
      swing_delays_odd_steps_(false), high_res_tick_counter_(0), next_trigger_tick_target_(0) {
  calculate_timing_params();
  printf("SequencerController: Initialized. Ticks/Step: %lu\n", high_res_ticks_per_step_);
}

// --- Public Methods ---

void SequencerController::calculate_timing_params() {
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

void SequencerController::set_swing_percent(uint8_t percent) {
  swing_percent_ = std::clamp(percent, static_cast<uint8_t>(50), static_cast<uint8_t>(75));
}

void SequencerController::set_swing_target(bool delay_odd) {
  swing_delays_odd_steps_ = delay_odd;
}

void SequencerController::reset() {
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
  bool step0_is_odd = (0 % 2) != 0; // False

  if (swing_delays_odd_steps_) {
    first_step_duration = step0_is_odd ? duration1 : duration2;
  } else {
    first_step_duration = step0_is_odd ? duration2 : duration1;
  }

  next_trigger_tick_target_ = std::max(1ul, static_cast<unsigned long>(first_step_duration));
}

bool SequencerController::start() {
  if (running) {
    printf("SequencerController: Already running\n");
    return false;
  }
  reset();
  tempo_source.add_observer(*this);
  running = true;
  printf("SequencerController: Started. Waiting for tick %llu\n", next_trigger_tick_target_);
  return true;
}

bool SequencerController::stop() {
  if (!running) {
    printf("SequencerController: Already stopped\n");
    return false;
  }
  tempo_source.remove_observer(*this);
  running = false;
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

void SequencerController::notification([[maybe_unused]] Tempo::SequencerTickEvent event) {
  if (!running)
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

[[nodiscard]] uint32_t SequencerController::get_current_step() const noexcept {
  const size_t num_steps = sequencer.get_num_steps();
  if (num_steps == 0)
    return 0;
  return current_step_counter % num_steps;
}

[[nodiscard]] bool SequencerController::is_running() const {
  return running;
}

void SequencerController::activate_repeat(uint32_t length) {
    if (running && !repeat_active_) {
        repeat_active_ = true;
        repeat_length_ = std::max(1u, length);
        const size_t num_steps = sequencer.get_num_steps();
        repeat_activation_step_index_ = (num_steps > 0) ? (current_step_counter % num_steps) : 0;
        repeat_activation_step_counter_ = current_step_counter;
        // printf("Repeat Activated: Length %lu, Start Step %lu (Abs Counter %llu)\n",
        //        repeat_length_, repeat_activation_step_index_, repeat_activation_step_counter_);
    }
}

void SequencerController::deactivate_repeat() {
    if (repeat_active_) {
        repeat_active_ = false;
        repeat_length_ = 0;
        // printf("Repeat Deactivated\n");
    }
}

void SequencerController::set_repeat_length(uint32_t length) {
    if (repeat_active_) {
        uint32_t new_length = std::max(1u, length);
        if (new_length != repeat_length_) {
             repeat_length_ = new_length;
             // printf("Repeat Length Changed: New Length %lu\n", repeat_length_);
        }
    }
}

[[nodiscard]] bool SequencerController::is_repeat_active() const {
    return repeat_active_;
}


} // namespace StepSequencer
