#ifndef DRUM_SEQUENCER_EFFECT_RANDOM_H
#define DRUM_SEQUENCER_EFFECT_RANDOM_H

#include "etl/array.h"
#include "musin/timing/step_sequencer.h"
#include <cstdint>

namespace drum {

template <size_t NumTracks, size_t NumSteps> class SequencerEffectRandom {
public:
  SequencerEffectRandom() = default;

  void generate_full_pattern(
      musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
      const etl::array<uint8_t, NumTracks> &active_notes);

  void randomize_continuous_step(
      musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
      const etl::array<uint8_t, NumTracks> &active_notes,
      uint64_t current_step_counter);

  void randomize_single_step_per_track(
      musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
      const etl::array<uint8_t, NumTracks> &active_notes);
};

} // namespace drum

#endif // DRUM_SEQUENCER_EFFECT_RANDOM_H
