#include "sequencer_storage.h"
#include "pico/time.h"
#include <cstdio>

namespace drum {

template <size_t NumTracks, size_t NumSteps>
SequencerStorage<NumTracks, NumSteps>::SequencerStorage() 
    : state_is_dirty_(false), last_change_time_ms_(0), last_save_time_ms_(0) {
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::save_state_to_flash(const SequencerPersistentState& state) {
  FILE* file = fopen(SEQUENCER_STATE_FILE, "wb");
  if (!file) {
    return false;
  }
  
  size_t written = fwrite(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);
  
  if (written == 1) {
    mark_state_clean();
    return true;
  }
  
  return false;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::load_state_from_flash(SequencerPersistentState& state) {
  FILE* file = fopen(SEQUENCER_STATE_FILE, "rb");
  if (!file) {
    return false; // File doesn't exist, not an error
  }
  
  size_t read_size = fread(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);
  
  if (read_size != 1 || !state.is_valid()) {
    return false; // Corrupted or invalid file
  }
  
  state_is_dirty_ = false;
  return true;
}

template <size_t NumTracks, size_t NumSteps>
void SequencerStorage<NumTracks, NumSteps>::mark_state_dirty() {
  state_is_dirty_ = true;
  last_change_time_ms_ = time_us_32() / 1000;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::should_save_now() const {
  if (!state_is_dirty_) {
    return false;
  }
  
  uint32_t current_time_ms = time_us_32() / 1000;
  uint32_t time_since_change = current_time_ms - last_change_time_ms_;
  uint32_t time_since_save = current_time_ms - last_save_time_ms_;
  
  // Save if enough time has passed since last change (debounce) 
  // OR if maximum interval has been exceeded
  return (time_since_change >= SAVE_DEBOUNCE_MS || 
          time_since_save >= MAX_SAVE_INTERVAL_MS);
}

template <size_t NumTracks, size_t NumSteps>
void SequencerStorage<NumTracks, NumSteps>::mark_state_clean() {
  state_is_dirty_ = false;
  last_save_time_ms_ = time_us_32() / 1000;
}

template <size_t NumTracks, size_t NumSteps>
bool SequencerStorage<NumTracks, NumSteps>::is_dirty() const {
  return state_is_dirty_;
}

// Explicit template instantiation for 4 tracks, 8 steps
template class SequencerStorage<4, 8>;

} // namespace drum