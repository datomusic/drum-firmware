#ifndef DRUM_SYSTEM_STATE_MACHINE_H
#define DRUM_SYSTEM_STATE_MACHINE_H

#include "drum/events.h"
#include "etl/observer.h"
#include "pico/time.h"
#include "system_state.h"
#include <memory>

namespace drum {

class PizzaDisplay; // Forward declaration

} // namespace drum

namespace musin {
class Logger; // Forward declaration
}

namespace drum {

/**
 * @brief State machine that manages system states using the State Pattern.
 *
 * This class coordinates state transitions and delegates state-specific
 * behavior to state objects. Dependencies are passed directly to states
 * to avoid unnecessary wrapper classes.
 */
class SystemStateMachine
    : public etl::observer<drum::Events::SysExTransferStateChangeEvent> {
public:
  /**
   * @brief Construct a new SystemStateMachine.
   * @param logger Reference to the logging system
   */
  SystemStateMachine(musin::Logger &logger);

  /**
   * @brief Update the current state.
   * @param now Current system time
   */
  void update(absolute_time_t now);

  /**
   * @brief Get the current state identifier.
   * @return SystemStateId The current state
   */
  SystemStateId get_current_state() const;

  /**
   * @brief Transition to a new state with validation.
   * @param new_state The target state to transition to
   * @return true if transition was successful, false if invalid
   */
  bool transition_to(SystemStateId new_state);

  /**
   * @brief Handle SysEx transfer state change events.
   * @param event The SysEx transfer state change event
   */
  void notification(drum::Events::SysExTransferStateChangeEvent event) override;

private:
  musin::Logger &logger_;
  std::unique_ptr<SystemState> current_state_;

  /**
   * @brief Validate if a state transition is allowed.
   * @param from Source state
   * @param to Target state
   * @return true if transition is valid, false otherwise
   */
  bool is_valid_transition(SystemStateId from, SystemStateId to) const;

  /**
   * @brief Create a state object for the given state ID.
   * @param state_id The state to create
   * @return std::unique_ptr<SystemState> The created state object
   */
  std::unique_ptr<SystemState> create_state(SystemStateId state_id) const;
};

} // namespace drum

#endif // DRUM_SYSTEM_STATE_MACHINE_H