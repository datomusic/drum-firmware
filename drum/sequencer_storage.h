#ifndef DRUM_SEQUENCER_STORAGE_H
#define DRUM_SEQUENCER_STORAGE_H

#include "config.h"
#include "save_timing_manager.h"
#include "sequencer_persistence.h"
#include <cstdint>

namespace drum {

/**
 * @brief Orchestrates sequencer state persistence.
 *
 * This class composes SaveTimingManager (for timing logic)
 * to provide a testable, modular architecture following SOLID principles.
 * The file I/O operations are handled internally.
 */
template <size_t NumTracks, size_t NumSteps> class SequencerStorage {
public:
  /**
   * @brief Constructor
   * @param state_path Path to state file (defaults to production path)
   */
  SequencerStorage(const char *state_path = "/sequencer_state.dat");

  /**
   * @brief Save sequencer state to persistent storage.
   * @param state The state data to save
   * @return true if save was successful, false otherwise
   */
  bool save_state_to_flash(const SequencerPersistentState &state);

  /**
   * @brief Load sequencer state from persistent storage.
   * @param state Output parameter to receive the loaded state
   * @return true if load was successful, false otherwise
   */
  bool load_state_from_flash(SequencerPersistentState &state);

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
  static constexpr uint32_t SAVE_DEBOUNCE_MS = 10000; // 10 second debounce
  static constexpr uint32_t MAX_SAVE_INTERVAL_MS =
      30000; // Maximum 30s between saves when dirty

  // File I/O operations
  bool save_to_file(const char *filepath,
                    const SequencerPersistentState &state);
  bool load_from_file(const char *filepath, SequencerPersistentState &state);

  // Composed architecture - testable components with dependency injection
  PicoTimeSource pico_time_;
  SaveTimingManager timing_manager_;
  const char *state_path_;
};

} // namespace drum

#endif // DRUM_SEQUENCER_STORAGE_H
