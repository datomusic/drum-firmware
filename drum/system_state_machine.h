#ifndef DRUM_SYSTEM_STATE_MACHINE_H
#define DRUM_SYSTEM_STATE_MACHINE_H

#include "drum/events.h"
#include "etl/observer.h"
#include "pico/time.h"

namespace drum {

enum class SystemState {
  Boot,         // Hardware initialization, runs boot animation
  Sequencer,    // Normal sequencer operation
  FileTransfer, // File transfer mode - minimal systems for performance
  Sleep         // Sleep mode - device is powered down, wake on playbutton
};

class SystemStateMachine
    : public etl::observer<drum::Events::SysExTransferStateChangeEvent> {
public:
  SystemStateMachine();

  void update(absolute_time_t now);
  SystemState get_current_state() const {
    return current_state_;
  }

  // Call this after initialization is complete to transition to Sequencer
  void initialization_complete();

  // Call this when playbutton is held to enter sleep mode
  void enter_sleep_mode();

  void notification(drum::Events::SysExTransferStateChangeEvent event) override;

private:
  SystemState current_state_;
  bool initialization_done_;

  void transition_to_sequencer();
  void transition_to_file_transfer();
  void transition_from_file_transfer();
};

} // namespace drum

#endif // DRUM_SYSTEM_STATE_MACHINE_H