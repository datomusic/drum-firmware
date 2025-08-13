#include "system_state_machine.h"

namespace drum {

SystemStateMachine::SystemStateMachine()
    : current_state_(SystemState::Boot), boot_start_time_(get_absolute_time()) {
}

void SystemStateMachine::update(absolute_time_t now) {
  switch (current_state_) {
  case SystemState::Boot:
    if (absolute_time_diff_us(boot_start_time_, now) >=
        BOOT_DURATION_MS * 1000) {
      transition_to_active();
    }
    break;
  case SystemState::Active:
    break;
  }
}

void SystemStateMachine::transition_to_active() {
  current_state_ = SystemState::Active;
}

void SystemStateMachine::notification(
    [[maybe_unused]] drum::Events::SysExTransferStateChangeEvent event) {
  // For now, we handle SysEx state changes within Active state
  // The display strategy pattern handles the visual switch between
  // sequencer and file transfer modes
}

} // namespace drum