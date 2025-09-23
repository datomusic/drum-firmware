#include "sequencer_effect_random.h"
#include "drum/config.h"
#include <cstdlib> // For rand()

namespace drum {

template <size_t NumTracks, size_t NumSteps>
void SequencerEffectRandom<NumTracks, NumSteps>::generate_full_pattern(
    musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
    const etl::array<uint8_t, NumTracks> &active_notes) {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    auto &random_track = sequencer.get_track(track_idx);

    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      auto &random_step = random_track.get_step(step_idx);

      random_step.note = active_notes[track_idx];

      uint32_t random_value = rand();
      random_step.velocity = random_value & 0x7F;
      random_step.enabled = (random_value & 0x40) != 0;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerEffectRandom<NumTracks, NumSteps>::randomize_continuous_step(
    musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
    const etl::array<uint8_t, NumTracks> &active_notes,
    uint64_t current_step_counter) {
  const size_t num_steps = sequencer.get_num_steps();
  if (num_steps > 0) {
    for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
      uint32_t random_value = rand();

      size_t track_offset = (random_value >> (8 + track_idx * 3)) & 0x7;
      size_t steps_ahead_index =
          (current_step_counter + track_offset) % num_steps;

      auto &random_track = sequencer.get_track(track_idx);
      auto &random_step = random_track.get_step(steps_ahead_index);

      random_step.note = active_notes[track_idx];

      bool should_enable = (random_value & 0x01) != 0;
      random_step.enabled = should_enable;
      random_step.velocity = (random_value >> 7) & 0x7F;
    }
  }
}

template <size_t NumTracks, size_t NumSteps>
void SequencerEffectRandom<NumTracks, NumSteps>::
    randomize_single_step_per_track(
        musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
        const etl::array<uint8_t, NumTracks> &active_notes) {
  const size_t num_steps = sequencer.get_num_steps();
  if (num_steps > 0) {
    for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
      uint32_t random_value = rand();

      // Pick a random step index for this track
      size_t random_step_index = random_value % num_steps;

      auto &track = sequencer.get_track(track_idx);
      auto &step = track.get_step(random_step_index);

      step.note = active_notes[track_idx];
      step.velocity = (random_value >> 7) & 0x7F;
      step.enabled = (random_value & 0x40) != 0;
    }
  }
}

template class SequencerEffectRandom<config::NUM_TRACKS,
                                     config::NUM_STEPS_PER_TRACK>;

} // namespace drum
