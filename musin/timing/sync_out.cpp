#include "sync_out.h"

#include "pico/stdlib.h" // For hardware_alarm functions
#include <cstdio>        // For printf, if debugging

namespace musin::timing {

SyncOut::SyncOut(std::uint32_t gpio_pin, musin::timing::InternalClock &clock_source,
                 std::uint32_t ticks_per_pulse, std::uint32_t pulse_duration_ms)
    : _gpio(gpio_pin),
      _clock_source(clock_source),
      _ticks_per_pulse((ticks_per_pulse == 0) ? 1 : ticks_per_pulse), // Default to 1 if 0 is passed
      _pulse_duration_us((pulse_duration_ms == 0)
                             ? 1000 // Default to 1ms if 0 is passed
                             : static_cast<std::uint64_t>(pulse_duration_ms) * 1000),
      _tick_counter(0),
      _is_enabled(false),
      _pulse_active(false),
      _pulse_alarm_id(0) {
  _gpio.set_direction(musin::hal::GpioDirection::OUT);
  _gpio.write(false); // Ensure output is initially low
}

SyncOut::~SyncOut() {
  disable(); // Ensure cleanup on destruction
}

void SyncOut::notification(musin::timing::ClockEvent /* event */) {
  if (!_is_enabled) {
    return;
  }

  _tick_counter++;
  if (_tick_counter >= _ticks_per_pulse) {
    _tick_counter = 0;

    if (!_pulse_active) {
      _gpio.write(true);
      _pulse_active = true;

      // Cancel any existing alarm before setting a new one (should not happen often)
      if (_pulse_alarm_id > 0) {
        cancel_alarm(_pulse_alarm_id);
      }
      // User_data is 'this' SyncOut instance
      _pulse_alarm_id = add_alarm_in_us(_pulse_duration_us, pulse_off_alarm_callback, this, true);
      if (_pulse_alarm_id <= 0) {
        // Handle error: Failed to add alarm.
        // For now, immediately turn off pulse to prevent it staying high indefinitely.
        // A more robust solution might involve logging or an error state.
        trigger_pulse_off();
        // printf("SyncOut Error: Failed to schedule pulse off alarm.\n");
      }
    }
    // If pulse is already active, this tick is effectively skipped for re-triggering
    // to prevent re-entrant calls or complex logic for extending pulse.
  }
}

void SyncOut::enable() {
  if (_is_enabled) {
    return;
  }
  _clock_source.add_observer(*this);
  _is_enabled = true;
  _tick_counter = 0; // Reset counter on enable
                     // printf("SyncOut: Enabled. Pin: %u\n", _gpio.get_pin_num()); // Assuming GpioPin has get_pin_num()
}

void SyncOut::disable() {
  if (!_is_enabled) {
    return;
  }
  _clock_source.remove_observer(*this);
  _is_enabled = false;

  if (_pulse_active) {
    if (_pulse_alarm_id > 0) {
      cancel_alarm(_pulse_alarm_id);
      _pulse_alarm_id = 0;
    }
    trigger_pulse_off(); // Ensure GPIO is low
  }
  // printf("SyncOut: Disabled. Pin: %u\n", _gpio.get_pin_num());
}

bool SyncOut::is_enabled() const {
  return _is_enabled;
}

// Static callback wrapper for the alarm
int64_t SyncOut::pulse_off_alarm_callback(alarm_id_t /* id */, void *user_data) {
  SyncOut *instance = static_cast<SyncOut *>(user_data);
  if (instance) {
    instance->trigger_pulse_off();
  }
  return 0; // Do not reschedule, it's a one-shot alarm
}

// Member function to handle turning the pulse off
void SyncOut::trigger_pulse_off() {
  _gpio.write(false);
  _pulse_active = false;
  _pulse_alarm_id = 0; // Mark alarm as no longer active or needed
}

} // namespace musin::timing
