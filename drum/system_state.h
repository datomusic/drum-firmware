#ifndef DRUM_SYSTEM_STATE_H
#define DRUM_SYSTEM_STATE_H

#include "pico/time.h"

namespace drum {

class SystemContext; // Forward declaration

enum class SystemStateId {
  Boot,
  Sequencer,
  FileTransfer,
  Sleep
};

/**
 * @brief Abstract base class for system states in the State Pattern.
 *
 * Each state encapsulates behavior for a specific system mode and manages
 * its own lifecycle through enter/update/exit methods.
 */
class SystemState {
public:
  virtual ~SystemState() = default;

  /**
   * @brief Called when entering this state.
   * @param context Shared resources and dependencies
   */
  virtual void enter(SystemContext &context) = 0;

  /**
   * @brief Called periodically while in this state.
   * @param context Shared resources and dependencies
   * @param now Current system time
   */
  virtual void update(SystemContext &context, absolute_time_t now) = 0;

  /**
   * @brief Called when exiting this state.
   * @param context Shared resources and dependencies
   */
  virtual void exit(SystemContext &context) = 0;

  /**
   * @brief Get the unique identifier for this state.
   * @return SystemStateId The state identifier
   */
  virtual SystemStateId get_id() const = 0;
};

} // namespace drum

#endif // DRUM_SYSTEM_STATE_H