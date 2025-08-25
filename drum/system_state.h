#ifndef DRUM_SYSTEM_STATE_H
#define DRUM_SYSTEM_STATE_H

#include "pico/time.h"

namespace drum {

class PizzaDisplay;       // Forward declaration
class SystemStateMachine; // Forward declaration

} // namespace drum

namespace musin {
class Logger; // Forward declaration
}

namespace drum {

enum class SystemStateId {
  Boot,
  Sequencer,
  FileTransfer,
  FallingAsleep,
  Sleep
};

/**
 * @brief Abstract base class for system states using the State Pattern.
 *
 * Each state encapsulates the behavior specific to that system state.
 * States receive direct dependencies as parameters to avoid unnecessary
 * wrapper classes and circular dependencies.
 */
class SystemState {
public:
  virtual ~SystemState() = default;

  /**
   * @brief Called when entering this state.
   * @param display Reference to the display system
   * @param logger Reference to the logging system
   */
  virtual void enter(PizzaDisplay &display, musin::Logger &logger) = 0;

  /**
   * @brief Called every update cycle while in this state.
   * @param display Reference to the display system
   * @param logger Reference to the logging system
   * @param state_machine Reference to state machine for transitions
   * @param now Current system time
   */
  virtual void update(PizzaDisplay &display, musin::Logger &logger,
                      SystemStateMachine &state_machine,
                      absolute_time_t now) = 0;

  /**
   * @brief Called when exiting this state.
   * @param display Reference to the display system
   * @param logger Reference to the logging system
   */
  virtual void exit(PizzaDisplay &display, musin::Logger &logger) = 0;

  /**
   * @brief Get the state identifier.
   * @return SystemStateId The state this object represents
   */
  virtual SystemStateId get_id() const = 0;
};

} // namespace drum

#endif // DRUM_SYSTEM_STATE_H