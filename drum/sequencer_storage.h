#ifndef DRUM_SEQUENCER_STORAGE_H
#define DRUM_SEQUENCER_STORAGE_H

#include "config.h"
#include "sequencer_persistence.h"
#include <cstdint>

namespace drum {

/**
 * @brief Handles sequencer state persistence to flash storage.
 * 
 * This class manages the saving and loading of sequencer state data,
 * providing debounced persistence with intelligent timing to minimize
 * flash wear while ensuring data integrity.
 */
template <size_t NumTracks, size_t NumSteps>
class SequencerStorage {
public:
  /**
   * @brief Constructor
   */
  SequencerStorage();

  /**
   * @brief Save sequencer state to persistent storage.
   * @param state The state data to save
   * @return true if save was successful, false otherwise
   */
  bool save_state_to_flash(const SequencerPersistentState& state);

  /**
   * @brief Load sequencer state from persistent storage.
   * @param state Output parameter to receive the loaded state
   * @return true if load was successful, false otherwise
   */
  bool load_state_from_flash(SequencerPersistentState& state);

  /**
   * @brief Mark the sequencer state as dirty (needs saving).
   * This starts the debounce timer for automatic persistence.
   */
  void mark_state_dirty();

  /**
   * @brief Check if state should be saved based on debounce logic.
   * Call this periodically to trigger saves when appropriate.
   * @return true if state should be saved now, false otherwise
   */
  bool should_save_now() const;

  /**
   * @brief Reset the dirty flag after successful save.
   */
  void mark_state_clean();

  /**
   * @brief Check if state is currently dirty.
   * @return true if state needs saving, false otherwise
   */
  bool is_dirty() const;

private:
  static constexpr const char* SEQUENCER_STATE_FILE = "/sequencer_state.dat";
  static constexpr uint32_t SAVE_DEBOUNCE_MS = 2000; // 2 second debounce
  static constexpr uint32_t MAX_SAVE_INTERVAL_MS = 30000; // Maximum 30s between saves when dirty
  
  bool state_is_dirty_ = false;
  uint32_t last_change_time_ms_ = 0;
  uint32_t last_save_time_ms_ = 0;
};

} // namespace drum

#endif // DRUM_SEQUENCER_STORAGE_H