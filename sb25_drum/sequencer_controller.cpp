#include "sequencer_controller.h"
#include "midi.h" // For send_midi_note
#include <cstdio> // For printf

namespace StepSequencer {

SequencerController::SequencerController(StepSequencer::Sequencer<4, 8> &sequencer_ref)
    : sequencer(sequencer_ref), current_step_counter(0) {
  printf("SequencerController: Initialized.\n");
}

void SequencerController::notification(Tempo::SequencerTickEvent event) {
  // This is called for every sequencer tick (e.g., every 16th note by default)

  // --- Calculate Step Index ---
  size_t num_steps = sequencer.get_num_steps(); // Should be 8 for Sequencer<4, 8>
  if (num_steps == 0)
    return; // Avoid division by zero if sequencer is invalid

  // Calculate the step index within the pattern (wrap around)
  size_t step_index_in_pattern = current_step_counter % num_steps;

  // printf("Sequencer Tick: Step %zu (Counter: %lu)\n", step_index_in_pattern,
  // current_step_counter);

  // --- Trigger Notes for the Current Step ---
  size_t num_tracks = sequencer.get_num_tracks();

  for (size_t track_idx = 0; track_idx < num_tracks; ++track_idx) {
    const Step &step = sequencer.get_track(track_idx).get_step(step_index_in_pattern);

    if (step.enabled && step.note.has_value() && step.velocity.has_value()) {
      // Send MIDI Note On
      // Assuming MIDI channel = track index + 1 (adjust as needed)
      uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);
      send_midi_note(midi_channel, step.note.value(), step.velocity.value());

      // printf("  Track %zu, Step %zu: Note On %u, Vel %u, Ch %u\n",
      //        track_idx, step_index_in_pattern, step.note.value(), step.velocity.value(),
      //        midi_channel);

      // TODO: Implement Note Off handling later (e.g., fixed duration, or separate Note Off events)
    }
  }

  // --- Advance Step Counter ---
  current_step_counter++;
  // Counter runs indefinitely unless reset
}

uint32_t SequencerController::get_current_step() const {
  size_t num_steps = sequencer.get_num_steps();
  if (num_steps == 0)
    return 0;
  return current_step_counter % num_steps; // Return step within pattern bounds
}

void SequencerController::reset() {
  current_step_counter = 0;
  printf("SequencerController: Reset current step counter to 0.\n");
  // TODO: Send MIDI Note Off for any sounding notes if necessary
}

} // namespace StepSequencer
