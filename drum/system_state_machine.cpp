#include "system_state_machine.h"
#include "state_implementations.h"

namespace drum {

SystemStateMachine::SystemStateMachine(SystemContext &context)
    : context_(context) {
  // Start in Boot state
  current_state_ = create_state(SystemStateId::Boot);
  current_state_->enter(context_);
}

void SystemStateMachine::update(absolute_time_t now) {
  if (current_state_) {
    current_state_->update(context_, now);
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
    context_.logger.error("Cannot transition: no current state");
    return false;
  }

  SystemStateId current_id = current_state_->get_id();

  // Validate transition
  if (!is_valid_transition(current_id, new_state)) {
    context_.logger.error("Invalid state transition");
    return false;
  }

  // Perform transition
  context_.logger.debug("State transition");

  current_state_->exit(context_);
  current_state_ = create_state(new_state);
  current_state_->enter(context_);

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
    return (to == SystemStateId::FileTransfer || to == SystemStateId::Sleep);

  case SystemStateId::FileTransfer:
    return (to == SystemStateId::Sequencer);

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

  case SystemStateId::Sleep:
    return std::make_unique<SleepState>();

  default:
    context_.logger.error("Unknown state ID");
    return std::make_unique<BootState>(); // Fallback to Boot
  }
}

} // namespace drum