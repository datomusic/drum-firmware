#include "internal_clock.h"

#include "musin/timing/clock_event.h"
#include "pico/stdlib.h"
#include <cstdio>

namespace musin::timing {

InternalClock::InternalClock(float initial_bpm)
    : _current_bpm(initial_bpm), _is_running(false),
      _bpm_change_pending{false} {
  _timer_info = {};
  _tick_interval_us = calculate_tick_interval(initial_bpm);
  _pending_bpm = initial_bpm;
  _pending_tick_interval_us = _tick_interval_us;
}

void InternalClock::set_bpm(float bpm) {
  if (bpm <= 0.0f) {
    return;
  }

  if (_bpm_change_pending.load(std::memory_order_relaxed)) {
    if (bpm == _pending_bpm) {
      return;
    }
  } else {
    if (bpm == _current_bpm) {
      return;
    }
  }

  int64_t new_interval = calculate_tick_interval(bpm);

  if (_is_running) {
    _pending_bpm = bpm;
    _pending_tick_interval_us = new_interval;
    _bpm_change_pending.store(true, std::memory_order_relaxed);
  } else {
    _current_bpm = bpm;
    _tick_interval_us = new_interval;
    _bpm_change_pending.store(false, std::memory_order_relaxed);
  }
}

float InternalClock::get_bpm() const {
  return _current_bpm;
}

void InternalClock::start() {
  if (_is_running) {
    return;
  }
  if (_tick_interval_us <= 0) {
    return;
  }

  // Add the repeating timer
  if (add_repeating_timer_us(-_tick_interval_us, timer_callback, this,
                             &_timer_info)) {
    _is_running = true;
  } else {
    _is_running = false;
  }
}

void InternalClock::stop() {
  if (!_is_running) {
    return;
  }

  // Cancel the repeating timer
  cancel_repeating_timer(&_timer_info);
  _is_running = false;
  _bpm_change_pending.store(false, std::memory_order_relaxed);

  _timer_info = {};
}

bool InternalClock::is_running() const {
  return _is_running;
}

void InternalClock::set_discipline(ClockSource source, uint32_t ppqn) {
  // TODO: To be implemented in Phase 1
}

int64_t InternalClock::calculate_tick_interval(float bpm) const {
  if (bpm <= 0.0f) {
    return 0;
  }
  float ticks_per_second = (bpm / 60.0f) * static_cast<float>(PPQN);
  if (ticks_per_second <= 0.0f) {
    return 0;
  }
  return static_cast<int64_t>(1000000.0f / ticks_per_second);
}

// --- Static Timer Callback ---
bool InternalClock::timer_callback(struct repeating_timer *rt) {
  InternalClock *instance = static_cast<InternalClock *>(rt->user_data);

  if (!instance->_is_running) {
    return false; // Stop the timer if the clock instance was stopped
  }

  // Notify observers for the current tick (based on the old interval)
  musin::timing::ClockEvent tick_event{musin::timing::ClockSource::INTERNAL};
  instance->notify_observers(tick_event);

  // Check for and apply pending BPM change for the *next* interval
  if (instance->_bpm_change_pending.load(std::memory_order_relaxed)) {
    int64_t new_interval = instance->_pending_tick_interval_us;
    float new_bpm = instance->_pending_bpm;

    instance->_current_bpm = new_bpm;
    instance->_tick_interval_us = new_interval;

    if (new_interval <= 0) {
      // Invalid interval, stop the clock
      instance->_is_running = false;
      instance->_bpm_change_pending.store(false, std::memory_order_relaxed);
      return false; // Stop the timer
    }
    rt->delay_us = -new_interval; // Set the next interval for the timer

    instance->_bpm_change_pending.store(false, std::memory_order_relaxed);
  }

  return instance->_is_running; // Continue if still running
}

} // namespace musin::timing
