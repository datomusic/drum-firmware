#include "sync_out.h"
#include "musin/timing/timing_constants.h"

#include "pico/stdlib.h" // For hardware_alarm functions
#include <cstdio>        // For printf, if debugging

namespace musin::timing {

SyncOut::SyncOut(std::uint32_t gpio_pin, std::uint32_t ticks_per_pulse,
                 std::uint32_t pulse_duration_ms)
    : _gpio(gpio_pin),
      _ticks_per_pulse((ticks_per_pulse == 0)
                           ? 1
                           : ticks_per_pulse), // Default to 1 if 0 is passed
      _pulse_duration_us((pulse_duration_ms == 0)
                             ? 1000 // Default to 1ms if 0 is passed
                             : static_cast<std::uint64_t>(pulse_duration_ms) *
                                   1000),
      _is_enabled(false), _pulse_active(false), _pulse_alarm_id(0) {
  _gpio.set_direction(musin::hal::GpioDirection::OUT);
  _gpio.write(false); // Ensure output is initially low
}

SyncOut::~SyncOut() {
  disable(); // Ensure cleanup on destruction
}

void SyncOut::notification(musin::timing::TempoEvent /*event*/) {
  // Deprecated for SyncOut timing: use raw ClockEvent notification instead.
  // Intentionally no-op to avoid coupling to speed-modified phases.
}

void SyncOut::notification(musin::timing::ClockEvent event) {
  if (!_is_enabled) {
    return;
  }

  if (event.is_resync) {
    reset_counters();
  }

  // Count raw 24 PPQN ticks and pulse every _ticks_per_pulse
  _raw_tick_mod_counter++;
  if ((_raw_tick_mod_counter % _ticks_per_pulse) == 0) {
    if (!_pulse_active) {
      _gpio.write(true);
      _pulse_active = true;

      if (_pulse_alarm_id > 0) {
        cancel_alarm(_pulse_alarm_id);
      }

      _pulse_alarm_id = add_alarm_in_us(_pulse_duration_us,
                                        pulse_off_alarm_callback, this, true);
      if (_pulse_alarm_id <= 0) {
        trigger_pulse_off();
      }
    }
  }
}

void SyncOut::enable() {
  if (_is_enabled) {
    return;
  }
  _is_enabled = true;
  // printf("SyncOut: Enabled. Pin: %u\n", _gpio.get_pin_num());
}

void SyncOut::disable() {
  if (!_is_enabled) {
    return;
  }
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
int64_t SyncOut::pulse_off_alarm_callback(alarm_id_t /* id */,
                                          void *user_data) {
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

void SyncOut::reset_counters() {
  _raw_tick_mod_counter = 0;
}

void SyncOut::resync() {
  reset_counters();
}

} // namespace musin::timing
