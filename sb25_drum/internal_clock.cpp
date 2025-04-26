#include "internal_clock.h"
#include "pico/stdlib.h" // Required for PICO_ASSERT
#include <cstdio>        // For printf

namespace Clock {

InternalClock::InternalClock(float initial_bpm) : _current_bpm(initial_bpm) {
  // Initial calculation, timer setup happens in init()
  calculate_interval();
}

bool InternalClock::init() {
  if (_initialized) {
    return true;
  }
  // Use the default alarm pool
  _alarm_pool = alarm_pool_get_default();
  if (!_alarm_pool) {
    printf("InternalClock Error: Failed to get default alarm pool.\n");
    return false;
  }
  printf("InternalClock: Initialized with default alarm pool. Initial BPM: %.2f, Interval: %lld "
         "us\n",
         _current_bpm, _tick_interval_us);
  _initialized = true;
  return true;
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

    // If running, restart the timer with the new interval
    if (_is_running && _alarm_id > 0) {
      cancel_alarm(_alarm_id);
      _alarm_id = 0; // Mark as inactive before rescheduling
      start();       // Restart with the new interval
    }
  }
}

float InternalClock::get_bpm() const {
  return _current_bpm;
}

void InternalClock::start() {
  if (!_initialized) {
    printf("InternalClock Error: Cannot start, not initialized.\n");
    return;
  }
  if (_is_running) {
    // printf("InternalClock: Already running.\n");
    return;
  }
  if (_tick_interval_us <= 0) {
    printf("InternalClock Error: Cannot start, invalid interval (%lld us).\n", _tick_interval_us);
    return;
  }

  // Schedule the first alarm. The callback will reschedule subsequent alarms.
  // Use add_alarm_in_us for the first one to avoid immediate callback if interval is short.
  // Subsequent calls are handled by returning the interval from the callback.
  _alarm_id = alarm_pool_add_alarm_in_us(_alarm_pool, _tick_interval_us, timer_callback, this, true);

  if (_alarm_id > 0) {
    _is_running = true;
    printf("InternalClock: Started. Alarm ID: %d\n", _alarm_id);
  } else {
    printf("InternalClock Error: Failed to add alarm.\n");
    _is_running = false; // Ensure state is correct
  }
}

void InternalClock::stop() {
  if (!_is_running) {
    // printf("InternalClock: Already stopped.\n");
    return;
  }

  if (_alarm_id > 0) {
    cancel_alarm(_alarm_id);
    _alarm_id = 0; // Mark as inactive
  }
  _is_running = false;
  printf("InternalClock: Stopped.\n");
}

bool InternalClock::is_running() const {
  return _is_running;
}

void InternalClock::calculate_interval() {
  if (_current_bpm <= 0.0f) {
    _tick_interval_us = 0;
    return;
  }
  // Ticks per second = (BPM / 60) * PPQN
  float ticks_per_second = (_current_bpm / 60.0f) * static_cast<float>(PPQN);
  if (ticks_per_second <= 0.0f) {
      _tick_interval_us = 0;
      return;
  }
  // Interval in seconds = 1.0 / ticks_per_second
  // Interval in microseconds = (1.0 / ticks_per_second) * 1,000,000
  _tick_interval_us = static_cast<int64_t>(1000000.0f / ticks_per_second);
}

// --- Static Timer Callback ---
int64_t InternalClock::timer_callback(alarm_id_t id, void *user_data) {
  // Retrieve the instance pointer
  InternalClock *instance = static_cast<InternalClock *>(user_data);
  PICO_ASSERT(instance != nullptr); // Should always have a valid instance

  // Call the instance method to handle the tick and get the next interval
  return instance->handle_tick();
}

// --- Instance Tick Handler ---
int64_t InternalClock::handle_tick() {
  if (!_is_running) {
    return 0; // Do not reschedule if stopped
  }

  // Create and notify observers with a ClockEvent
  Clock::ClockEvent tick_event;
  // TODO: Populate event with timestamp or source if needed
  // tick_event.timestamp_us = time_us_64();
  // tick_event.source = ClockSource::INTERNAL; // Requires adding source to ClockEvent

  etl::observable<etl::observer<ClockEvent>, MAX_CLOCK_OBSERVERS>::notify_observers(tick_event);

  // Return the interval for the next callback, keeps the timer repeating
  return _tick_interval_us;
}

} // namespace Clock
