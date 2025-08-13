#ifndef DRUM_SYSTEM_CONTEXT_H
#define DRUM_SYSTEM_CONTEXT_H

#include "musin/hal/logger.h"

namespace drum {

class PizzaDisplay;       // Forward declaration
class SystemStateMachine; // Forward declaration

/**
 * @brief Context object that holds shared resources and dependencies for state
 * objects.
 *
 * This class provides dependency injection for state objects, allowing them to
 * access shared resources without tight coupling. Dependencies are passed as
 * references and are not owned by this context.
 */
class SystemContext {
public:
  /**
   * @brief Construct a new SystemContext with required dependencies.
   * @param display_ref Reference to the display system
   * @param logger_ref Reference to the logging system
   */
  SystemContext(PizzaDisplay &display_ref, musin::Logger &logger_ref)
      : display(display_ref), logger(logger_ref), state_machine(nullptr) {
  }

  /**
   * @brief Set the state machine reference (called after construction).
   * @param state_machine_ref Reference to the state machine
   */
  void set_state_machine(SystemStateMachine &state_machine_ref) {
    state_machine = &state_machine_ref;
  }

  // Non-copyable and non-movable
  SystemContext(const SystemContext &) = delete;
  SystemContext &operator=(const SystemContext &) = delete;
  SystemContext(SystemContext &&) = delete;
  SystemContext &operator=(SystemContext &&) = delete;

  // References to shared dependencies (not owned)
  PizzaDisplay &display;
  musin::Logger &logger;
  SystemStateMachine
      *state_machine; // Set after construction to avoid circular dependency

  // TODO: Add more dependencies in later phases:
  // - EventBus for state transitions
  // - Hardware abstraction classes (SleepManager, MuxController)
  // - Other shared resources as needed
};

} // namespace drum

#endif // DRUM_SYSTEM_CONTEXT_H