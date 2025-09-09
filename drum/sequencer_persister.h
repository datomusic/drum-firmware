#ifndef DRUM_SEQUENCER_PERSISTER_H
#define DRUM_SEQUENCER_PERSISTER_H

#include "sequencer_persistence.h"

namespace drum {

/**
 * @brief Pure file I/O operations for sequencer state persistence.
 * 
 * This class handles only the reading and writing of sequencer state
 * to/from files. It has no dependencies on timing, hardware, or state
 * management - making it easily testable.
 */
class SequencerPersister {
public:
  /**
   * @brief Save sequencer state to a file.
   * @param filepath Path to the file to write
   * @param state The state data to save
   * @return true if save was successful, false otherwise
   */
  bool save_to_file(const char* filepath, const SequencerPersistentState& state);

  /**
   * @brief Load sequencer state from a file.
   * @param filepath Path to the file to read
   * @param state Output parameter to receive the loaded state
   * @return true if load was successful, false otherwise
   */
  bool load_from_file(const char* filepath, SequencerPersistentState& state);
};

} // namespace drum

#endif // DRUM_SEQUENCER_PERSISTER_H