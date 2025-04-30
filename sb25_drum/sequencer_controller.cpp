#include "sequencer_controller.h"
#include "midi.h" // For send_midi_note
#include <cstdio> // For printf

namespace StepSequencer {

SequencerController::SequencerController(StepSequencer::Sequencer<4, 8> &sequencer_ref)
    : sequencer(sequencer_ref), current_step_counter(0), last_played_note_per_track{} {
  // Initialize last_played_note_per_track elements to std::nullopt by default
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
    uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1); // MIDI channel 1-based

    // --- Send Note Off for the previous note on this track (if any) ---
    if (last_played_note_per_track[track_idx].has_value()) {
      send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0); // Velocity 0 = Note Off
      last_played_note_per_track[track_idx] = std::nullopt; // Clear the stored note
    }

    // --- Process the current step ---
    const Step &step = sequencer.get_track(track_idx).get_step(step_index_in_pattern);

    if (step.enabled && step.note.has_value() && step.velocity.has_value() &&
        step.velocity.value() > 0) { // Ensure velocity is > 0 for Note On
      // Send MIDI Note On
      send_midi_note(midi_channel, step.note.value(), step.velocity.value());
      // Store the note that was just turned on
      last_played_note_per_track[track_idx] = step.note.value();
    }
    // If step is disabled, has no note/velocity, or velocity is 0,
    // the previous note (if any) was already turned off above,
    // and last_played_note_per_track[track_idx] remains nullopt.
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
  printf("SequencerController: Resetting. Sending Note Off for active notes.\n");
  // Send Note Off for any notes that were left playing
  for (size_t track_idx = 0; track_idx < last_played_note_per_track.size(); ++track_idx) {
    if (last_played_note_per_track[track_idx].has_value()) {
      uint8_t midi_channel = static_cast<uint8_t>(track_idx + 1);
      send_midi_note(midi_channel, last_played_note_per_track[track_idx].value(), 0); // Velocity 0
      last_played_note_per_track[track_idx] = std::nullopt;
    }
  }
  current_step_counter = 0;
}

} // namespace StepSequencer
