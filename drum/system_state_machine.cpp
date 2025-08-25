#include "system_state_machine.h"
#include "musin/hal/logger.h"
#include "state_implementations.h"

namespace drum {

SystemStateMachine::SystemStateMachine(PizzaDisplay &display,
                                       musin::Logger &logger)
    : display_(display), logger_(logger) {
  // Start in Boot state
  current_state_ = create_state(SystemStateId::Boot);
  current_state_->enter(display_, logger_);
}

void SystemStateMachine::update(absolute_time_t now) {
  if (current_state_) {
    current_state_->update(display_, logger_, *this, now);
  }
}

SystemStateId SystemStateMachine::get_current_state() const {
  if (current_state_) {
    return current_state_->get_id();
  }
  return SystemStateId::Boot; // Fallback
}

bool SystemStateMachine::transition_to(SystemStateId new_state) {
  if (!current_state_) {
    logger_.error("Cannot transition: no current state");
    return false;
  }

  SystemStateId current_id = current_state_->get_id();

  // Validate transition
  if (!is_valid_transition(current_id, new_state)) {
    logger_.error("Invalid state transition");
    return false;
  }

  // Perform transition

  current_state_->exit(display_, logger_);
  current_state_ = create_state(new_state);
  current_state_->enter(display_, logger_);

  return true;
}

void SystemStateMachine::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  // Handle SysEx transfer state changes
  if (event.is_active) {
    transition_to(SystemStateId::FileTransfer);
  } else {
    transition_to(SystemStateId::Sequencer);
  }
}

bool SystemStateMachine::is_valid_transition(SystemStateId from,
                                             SystemStateId to) const {
  // Transition validation table
  switch (from) {
  case SystemStateId::Boot:
    return (to == SystemStateId::Sequencer);

  case SystemStateId::Sequencer:
    return (to == SystemStateId::FileTransfer ||
            to == SystemStateId::FallingAsleep);

  case SystemStateId::FileTransfer:
    return (to == SystemStateId::Sequencer);

  case SystemStateId::FallingAsleep:
    return (to == SystemStateId::Sleep);

  case SystemStateId::Sleep:
    // Sleep state should trigger system reset, not normal transitions
    return false;

  default:
    return false;
  }
}

std::unique_ptr<SystemState>
SystemStateMachine::create_state(SystemStateId state_id) const {
  switch (state_id) {
  case SystemStateId::Boot:
    return std::make_unique<BootState>();

  case SystemStateId::Sequencer:
    return std::make_unique<SequencerState>();

  case SystemStateId::FileTransfer:
    return std::make_unique<FileTransferState>();

  case SystemStateId::FallingAsleep:
    return std::make_unique<FallingAsleepState>();

  case SystemStateId::Sleep:
    return std::make_unique<SleepState>();

  default:
    logger_.error("Unknown state ID");
    return std::make_unique<BootState>(); // Fallback to Boot
  }
}

} // namespace drum