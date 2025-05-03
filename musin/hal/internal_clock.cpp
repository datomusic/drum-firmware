#include "musin/hal/internal_clock.h"
#include "pico/stdlib.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_handler.h" // Added for ClockSource definition
#include <cstdio>

namespace Musin::HAL {

InternalClock::InternalClock(float initial_bpm) : _current_bpm(initial_bpm), _is_running(false) {
  _timer_info = {};
  calculate_interval();
}

void InternalClock::set_bpm(float bpm) {
  if (bpm <= 0.0f) {
    printf("InternalClock Warning: Ignoring invalid BPM value: %.2f\n", bpm);
    return;
  }

  if (bpm != _current_bpm) {
    _current_bpm = bpm;
    calculate_interval();
    printf("InternalClock: BPM set to %.2f, Interval updated to %lld us\n", _current_bpm,
           _tick_interval_us);

    if (_is_running) {
      stop();
      start();
    }
  }
}

float InternalClock::get_bpm() const {
  return _current_bpm;
}

void InternalClock::start() {
  if (_is_running) {
    printf("InternalClock: Already running.\n");
    return;
  }
  if (_tick_interval_us <= 0) {
    printf("InternalClock Error: Cannot start, invalid interval (%lld us).\n", _tick_interval_us);
    return;
  }

  // Add the repeating timer
  // Pass 'this' as user_data so the static callback can access the instance
  // The delay is negative to indicate the time *between starts* of callbacks
  if (add_repeating_timer_us(-_tick_interval_us, timer_callback, this, &_timer_info)) {
    _is_running = true;
    printf("InternalClock: Started. Interval: %lld us\n", _tick_interval_us);
  } else {
    printf("InternalClock Error: Failed to add repeating timer.\n");
    _is_running = false;
  }
}

void InternalClock::stop() {
  if (!_is_running) {
    printf("InternalClock: Already stopped.\n");
    return;
  }

  // Cancel the repeating timer
  bool cancelled = cancel_repeating_timer(&_timer_info);
  _is_running = false; // Assume stopped even if cancel fails (shouldn't happen often)

  if (cancelled) {
    printf("InternalClock: Stopped.\n");
  } else {
    // This might happen if the timer fired and the callback returned false
    // between the check for _is_running and the cancel_repeating_timer call,
    // or if the timer wasn't validly added in the first place.
    printf("InternalClock Warning: cancel_repeating_timer failed (timer might have already "
           "stopped).\n");
  }
  _timer_info = {};
}

bool InternalClock::is_running() const {
  return _is_running;
}

void InternalClock::calculate_interval() {
  if (_current_bpm <= 0.0f) {
    _tick_interval_us = 0;
    return;
  }
  float ticks_per_second = (_current_bpm / 60.0f) * static_cast<float>(PPQN);
  if (ticks_per_second <= 0.0f) {
    _tick_interval_us = 0;
    return;
  }
  _tick_interval_us = static_cast<int64_t>(1000000.0f / ticks_per_second);
}

// --- Static Timer Callback ---
// Signature matches repeating_timer_callback_t
bool InternalClock::timer_callback(struct repeating_timer *rt) {
  // Retrieve the instance pointer from user_data
  InternalClock *instance = static_cast<InternalClock *>(rt->user_data);
  // Assuming instance is always valid as per design constraints.

  // Check if the instance thinks it should still be running
  if (!instance->_is_running) {
    printf("InternalClock Callback: Instance stopped, cancelling timer.\n");
    return false; // Stop the timer
  }

  // Create and notify observers with a ClockEvent, specifying the source
  Musin::Timing::ClockEvent tick_event{Musin::Timing::ClockSource::INTERNAL};
  instance->notify_observers(tick_event);

  // Return true to continue the repeating timer
  return true;
}

// handle_tick() method removed

} // namespace Musin::HAL
