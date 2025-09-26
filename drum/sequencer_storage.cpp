#include "sequencer_storage.h"

namespace drum {

template <size_t NumTracks, size_t NumSteps>
SequencerStorage<NumTracks, NumSteps>::SequencerStorage()
    : persister_(), pico_time_(),
      timing_manager_(pico_time_, SAVE_DEBOUNCE_MS, MAX_SAVE_INTERVAL_MS) {
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::save_state_to_flash(
    const SequencerPersistentState &state) {
  bool success = persister_.save_to_file(SEQUENCER_STATE_FILE, state);
  if (success) {
    timing_manager_.mark_clean();
  }
  return success;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::load_state_from_flash(
    SequencerPersistentState &state) {
  bool success = persister_.load_from_file(SEQUENCER_STATE_FILE, state);
  if (success) {
    // Loading doesn't make state dirty, but ensure timing manager is clean
    timing_manager_.mark_clean();
  }
  return success;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerStorage<NumTracks, NumSteps>::mark_state_dirty() {
  timing_manager_.mark_dirty();
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::should_save_now() const {
  return timing_manager_.should_save_now();
}

template <size_t NumTracks, size_t NumSteps>
void SequencerStorage<NumTracks, NumSteps>::mark_state_clean() {
  timing_manager_.mark_clean();
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::is_dirty() const {
  return timing_manager_.is_dirty();
}

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerStorage<4, 8>;

} // namespace drum