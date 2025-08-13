#include "system_state_machine.h"
#include "drum/ui/pizza_display.h"

namespace drum {

SystemStateMachine::SystemStateMachine(PizzaDisplay &display_ref)
    : current_state_(SystemState::Boot), initialization_done_(false),
      _display_ref(display_ref) {
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
  case SystemState::Sleep:
    // Sleep state - device should be in infinite loop waiting for watchdog
    break;
  }
}

void SystemStateMachine::initialization_complete() {
  initialization_done_ = true;
}

void SystemStateMachine::enter_sleep_mode() {
  current_state_ = SystemState::Sleep;
  _display_ref.start_sleep_mode();
}

void SystemStateMachine::transition_to_sequencer() {
  current_state_ = SystemState::Sequencer;
  _display_ref.switch_to_sequencer_mode();
}

void SystemStateMachine::transition_to_file_transfer() {
  current_state_ = SystemState::FileTransfer;
  _display_ref.switch_to_file_transfer_mode();
}

void SystemStateMachine::transition_from_file_transfer() {
  current_state_ = SystemState::Sequencer;
  _display_ref.switch_to_sequencer_mode();
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