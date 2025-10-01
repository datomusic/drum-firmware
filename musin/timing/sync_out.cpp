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
  // Initialize countdown so first pulse happens after _ticks_per_pulse ticks
  _ticks_until_pulse = _ticks_per_pulse;
}

SyncOut::~SyncOut() {
  disable(); // Ensure cleanup on destruction
}

void SyncOut::notification(musin::timing::ClockEvent event) {
  if (!_is_enabled) {
    return;
  }

  if (event.is_resync) {
    if (_pulse_active) {
      if (_pulse_alarm_id > 0) {
        cancel_alarm(_pulse_alarm_id);
        _pulse_alarm_id = 0;
      }
      trigger_pulse_off();
    }
    // Treat the resync tick as an immediate downbeat pulse
    _ticks_until_pulse = 0;
  }

  // Align SyncOut pulse timing to physical sync boundaries
  if (event.is_physical_pulse) {
    _ticks_until_pulse = 0;
  }

  // Countdown raw 24 PPQN ticks and pulse when reaching zero
  if (_ticks_until_pulse > 0) {
    _ticks_until_pulse--;
  }
  if (_ticks_until_pulse == 0) {
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
    // Reset countdown for the next pulse window
    _ticks_until_pulse = _ticks_per_pulse;
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
  // Align countdown so next pulse is after a full window
  _ticks_until_pulse = _ticks_per_pulse;
}

void SyncOut::resync() {
  reset_counters();
}

} // namespace musin::timing
