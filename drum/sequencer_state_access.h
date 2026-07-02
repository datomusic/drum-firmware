#ifndef DRUM_SEQUENCER_STATE_ACCESS_H
#define DRUM_SEQUENCER_STATE_ACCESS_H

#include "drum/sequencer_persistence.h"

namespace drum {

/**
 * @brief Abstract accessor for reading/writing sequencer pattern state.
 *
 * Decouples consumers (e.g. SysExHandler) from the concrete
 * SequencerController template instantiation, avoiding template coupling
 * and void* casts across module boundaries.
 */
class SequencerStateAccess {
public:
  virtual ~SequencerStateAccess() = default;

  /**
   * @brief Gets the current sequencer state for external transfer.
   * @return The current persistent state including all track velocities and
   * active notes.
   */
  [[nodiscard]] virtual SequencerPersistentState get_current_state() const = 0;

  /**
   * @brief Applies an externally-provided sequencer state.
   * @param state The state to apply to the sequencer.
   * @return true if state was applied successfully, false on error.
   */
  virtual bool apply_state(const SequencerPersistentState &state) = 0;
};

} // namespace drum

#endif // DRUM_SEQUENCER_STATE_ACCESS_H
