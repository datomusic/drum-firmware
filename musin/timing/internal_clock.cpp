#include "internal_clock.h"

#include "musin/timing/clock_event.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <cstdio>

namespace musin::timing {

InternalClock::InternalClock(float initial_bpm)
    : _current_bpm(initial_bpm), _is_running(false) {
  _tick_interval_us = calculate_tick_interval(initial_bpm);
  _next_tick_time = nil_time;
}

void InternalClock::set_bpm(float bpm) {
  if (bpm <= 0.0f || bpm == _current_bpm) {
    return;
  }

  _current_bpm = bpm;
  int64_t new_interval = calculate_tick_interval(bpm);

  if (_is_running) {
    // To make the tempo change feel immediate, we adjust the next tick time
    // based on the time of the previously scheduled tick.
    absolute_time_t last_tick_time =
        delayed_by_us(_next_tick_time, -_tick_interval_us);
    _next_tick_time = delayed_by_us(last_tick_time, new_interval);
  }

  _tick_interval_us = new_interval;
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
  _is_running = true;
  _next_tick_time = delayed_by_us(get_absolute_time(), _tick_interval_us);
}

void InternalClock::stop() {
  if (!_is_running) {
    return;
  }
  _is_running = false;
  _next_tick_time = nil_time;
}

bool InternalClock::is_running() const {
  return _is_running;
}

void InternalClock::update(absolute_time_t now) {
  if (!_is_running || is_nil_time(_next_tick_time) ||
      !time_reached(_next_tick_time)) {
    return;
  }

  musin::timing::ClockEvent tick_event{musin::timing::ClockSource::INTERNAL};
  // Stamp with the scheduled tick time for determinism
  tick_event.timestamp_us = to_us_since_boot(_next_tick_time);
  notify_observers(tick_event);

  // Schedule the next tick.
  // To avoid drift, we schedule it relative to the last scheduled time,
  // not the current time 'now'.
  _next_tick_time = delayed_by_us(_next_tick_time, _tick_interval_us);
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

} // namespace musin::timing
