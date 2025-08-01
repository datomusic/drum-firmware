#include "musin/timing/sync_in.h"
#include "pico/time.h"

namespace musin::timing {

SyncIn::SyncIn(uint32_t sync_pin, uint32_t detect_pin)
    : sync_pin_(sync_pin), detect_pin_(detect_pin) {}

void SyncIn::init() {
  sync_pin_.init(musin::hal::GpioDirection::IN, false);
  detect_pin_.init(musin::hal::GpioDirection::IN, true); // Enable pull-down
  last_pulse_state_ = sync_pin_.read();
  last_detect_state_ = detect_pin_.read();
  current_detect_state_ = last_detect_state_;
  last_detect_change_time_ = nil_time;
  last_pulse_time_ = nil_time;
}

void SyncIn::update(absolute_time_t now) {
  // Handle first-time initialization of timers
  if (is_nil_time(last_pulse_time_)) {
    last_pulse_time_ = now;
  }
  if (is_nil_time(last_detect_change_time_)) {
    last_detect_change_time_ = now;
  }

  // Polling for sync pulse rising edge
  bool current_pulse_state = sync_pin_.read();
  if (current_pulse_state && !last_pulse_state_) { // Rising edge
    if (absolute_time_diff_us(last_pulse_time_, now) > PULSE_COOLDOWN_US) {
      ClockEvent event{ClockSource::EXTERNAL_SYNC};
      notify_observers(event);
      last_pulse_time_ = now;
    }
  }
  last_pulse_state_ = current_pulse_state;

  // Debouncing for cable detection
  bool raw_detect_state = detect_pin_.read();
  if (raw_detect_state != last_detect_state_) {
    last_detect_change_time_ = now;
  }
  last_detect_state_ = raw_detect_state;

  if (absolute_time_diff_us(last_detect_change_time_, now) >
      DETECT_DEBOUNCE_US) {
    current_detect_state_ = raw_detect_state;
  }
}

bool SyncIn::is_cable_connected() const { return current_detect_state_; }

} // namespace musin::timing
