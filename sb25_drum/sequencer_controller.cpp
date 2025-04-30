#include "sequencer_controller.h"
#include "midi.h" // For send_midi_note
#include <cstdio> // For printf

namespace StepSequencer {

SequencerController::SequencerController(StepSequencer::Sequencer<4, 8> &sequencer_ref)
    : sequencer(sequencer_ref), current_step_counter(0), last_played_note_per_track{} {
  // Initialize last_played_note_per_track elements to std::nullopt by default
  printf("SequencerController: Initialized with %zu tracks and %zu steps\n",
         sequencer.get_num_tracks(), sequencer.get_num_steps());
}

void SequencerController::notification([[maybe_unused]] Tempo::SequencerTickEvent event) {
  size_t num_steps = sequencer.get_num_steps();
  const size_t base_step = current_step_counter % num_steps;

  // printf("Sequencer Tick: Step %zu (Counter: %lu)\n", base_step, current_step_counter);

  size_t num_tracks = sequencer.get_num_tracks();

  for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
    uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);

    if (last_played_note_per_track[track_idx].has_value()) {
      send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0);
      last_played_note_per_track[track_idx] = std::nullopt;
    }

    const int effective_step = static_cast<int>(base_step) + track_offsets_[track_idx];
    const size_t wrapped_step =
        (effective_step % static_cast<int>(num_steps) + num_steps) % num_steps;
    const Step &step = sequencer.get_track(track_idx).get_step(wrapped_step);

    if (step.enabled && step.note.has_value() && step.velocity.has_value() &&
        step.velocity.value() > 0) {
      send_midi_note(midi_channel, step.note.value(), step.velocity.value());
      last_played_note_per_track[track_idx] = step.note.value();
    }
  }

  current_step_counter++;
}

[[nodiscard]] uint32_t SequencerController::get_current_step() const noexcept {
  const size_t num_steps = sequencer.get_num_steps();
  return current_step_counter % num_steps;
}

void SequencerController::reset() {
  printf("SequencerController: Resetting. Sending Note Off for active notes.\n");
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);
      send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0);
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  current_step_counter = 0;
}

} // namespace StepSequencer
