#ifndef DRUM_SYSTEM_STATE_MACHINE_H
#define DRUM_SYSTEM_STATE_MACHINE_H

#include "drum/events.h"
#include "etl/observer.h"
#include "pico/time.h"

namespace drum {

enum class SystemState {
  Boot,  // Hardware initialization, runs boot animation
  Active // Normal sequencer/file transfer operation
};

class SystemStateMachine
    : public etl::observer<drum::Events::SysExTransferStateChangeEvent> {
public:
  SystemStateMachine();

  void update(absolute_time_t now);
  SystemState get_current_state() const {
    return current_state_;
  }

  void notification(drum::Events::SysExTransferStateChangeEvent event) override;

private:
  SystemState current_state_;
  absolute_time_t boot_start_time_;

  static constexpr uint32_t BOOT_DURATION_MS = 2000;

  void transition_to_active();
};

} // namespace drum

#endif // DRUM_SYSTEM_STATE_MACHINE_H