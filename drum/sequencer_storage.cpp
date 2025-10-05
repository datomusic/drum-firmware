#include "sequencer_storage.h"
#include <cstdio>

namespace drum {

template <size_t NumTracks, size_t NumSteps>
SequencerStorage<NumTracks, NumSteps>::SequencerStorage()
    : pico_time_(),
      timing_manager_(pico_time_, SAVE_DEBOUNCE_MS, MAX_SAVE_INTERVAL_MS) {
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::save_state_to_flash(
    const SequencerPersistentState &state) {
  bool success = save_to_file(SEQUENCER_STATE_FILE, state);
  if (success) {
    timing_manager_.mark_clean();
  }
  return success;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::load_state_from_flash(
    SequencerPersistentState &state) {
  bool success = load_from_file(SEQUENCER_STATE_FILE, state);
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

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::save_to_file(
    const char *filepath, const SequencerPersistentState &state) {
  FILE *file = fopen(filepath, "wb");
  if (!file) {
    return false;
  }

  size_t written = fwrite(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);

  return written == 1;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::load_from_file(
    const char *filepath, SequencerPersistentState &state) {
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    return false; // File doesn't exist, not an error
  }

  size_t read_size = fread(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);

  if (read_size != 1 || !state.is_valid()) {
    return false; // Corrupted or invalid file
  }

  return true;
}

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerStorage<4, 8>;

} // namespace drum
