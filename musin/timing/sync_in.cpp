#include "musin/timing/sync_in.h"
#include "pico/time.h"

namespace musin::timing {

SyncIn::SyncIn(uint32_t sync_pin, uint32_t detect_pin)
    : sync_pin_(sync_pin), detect_pin_(detect_pin) {
  sync_pin_.set_direction(musin::hal::GpioDirection::IN);
  sync_pin_.disable_pulls();

  detect_pin_.set_direction(musin::hal::GpioDirection::IN);
  detect_pin_.disable_pulls(); // Rely on external pull-up

  // Initialize cable detection state
  last_detect_state_ = detect_pin_.read();
  current_detect_state_ = last_detect_state_;
  last_detect_change_time_ = nil_time;

  // Initialize pulse detection state based on the pin's state at startup
  pulse_state_ = sync_pin_.read() ? PulseDebounceState::WAITING_FOR_STABLE_LOW
                                  : PulseDebounceState::WAITING_FOR_RISING_EDGE;
  falling_edge_time_ = nil_time;
}

void SyncIn::update(absolute_time_t now) {
  // --- Pulse Debouncing Logic ---
  bool current_pulse_pin_state = sync_pin_.read();

  switch (pulse_state_) {
  case PulseDebounceState::WAITING_FOR_RISING_EDGE:
    if (current_pulse_pin_state) { // Rising edge detected
      // Fire the event immediately, marking it as a physical pulse
      ClockEvent event{ClockSource::EXTERNAL_SYNC};
      event.is_physical_pulse = true;
      event.timestamp_us = static_cast<uint32_t>(to_us_since_boot(now));
      notify_observers(event);

      // Transition to the next state to wait for a stable low
      pulse_state_ = PulseDebounceState::WAITING_FOR_STABLE_LOW;
      falling_edge_time_ = nil_time; // Ensure this is reset
    }
    break;

  case PulseDebounceState::WAITING_FOR_STABLE_LOW:
    if (!current_pulse_pin_state) { // Pin is now low
      // If this is the first time we see a low, record the time
      if (is_nil_time(falling_edge_time_)) {
        falling_edge_time_ = now;
      }

      // Check if it has been low for the required debounce duration
      if (absolute_time_diff_us(falling_edge_time_, now) > PULSE_DEBOUNCE_US) {
        pulse_state_ = PulseDebounceState::WAITING_FOR_RISING_EDGE;
      }
    } else {
      // Pin went high again, so it was a bounce. Reset the timer.
      falling_edge_time_ = nil_time;
    }
    break;
  }

  // --- Cable Detection Debouncing Logic ---
  if (is_nil_time(last_detect_change_time_)) {
    last_detect_change_time_ = now;
  }

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

bool SyncIn::is_cable_connected() const {
  return !current_detect_state_; // Active low: true when pin is low
}

} // namespace musin::timing
