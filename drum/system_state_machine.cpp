#include "system_state_machine.h"

namespace drum {

SystemStateMachine::SystemStateMachine()
    : current_state_(SystemState::Boot), initialization_done_(false) {
}

void SystemStateMachine::update([[maybe_unused]] absolute_time_t now) {
  switch (current_state_) {
  case SystemState::Boot:
    if (initialization_done_) {
      transition_to_sequencer();
    }
    break;
  case SystemState::Sequencer:
    break;
  case SystemState::FileTransfer:
    break;
  }
}

void SystemStateMachine::initialization_complete() {
  initialization_done_ = true;
}

void SystemStateMachine::transition_to_sequencer() {
  current_state_ = SystemState::Sequencer;
}

void SystemStateMachine::transition_to_file_transfer() {
  current_state_ = SystemState::FileTransfer;
}

void SystemStateMachine::transition_from_file_transfer() {
  current_state_ = SystemState::Sequencer;
}

void SystemStateMachine::notification(
    drum::Events::SysExTransferStateChangeEvent event) {
  // Only handle state changes if we're not in Boot mode
  if (current_state_ == SystemState::Boot) {
    return;
  }

  if (event.is_active) {
    transition_to_file_transfer();
  } else {
    transition_from_file_transfer();
  }
}

} // namespace drum