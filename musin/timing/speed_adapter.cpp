#include "musin/timing/speed_adapter.h"

namespace musin::timing {

void SpeedAdapter::notification(musin::timing::ClockEvent event) {
  current_source_ = event.source;

  if (event.is_resync) {
    // Forward resync immediately and reset internal state
    notify_observers(event);
    half_toggle_ = false;
    last_tick_time_ = nil_time;
    last_interval_us_ = 0;
    next_insert_time_ = nil_time;
    return;
  }

  absolute_time_t now = get_absolute_time();

  switch (modifier_) {
  case SpeedModifier::NORMAL_SPEED: {
    notify_observers(event);
    // Track interval for potential future DOUBLE transitions
    if (!is_nil_time(last_tick_time_)) {
      last_interval_us_ = absolute_time_diff_us(last_tick_time_, now);
    }
    last_tick_time_ = now;
    break;
  }
  case SpeedModifier::HALF_SPEED: {
    // Toggle and only forward on every other tick
    half_toggle_ = !half_toggle_;
    if (half_toggle_) {
      notify_observers(event);
    }
    // Track timing regardless to keep continuity when switching modes
    if (!is_nil_time(last_tick_time_)) {
      last_interval_us_ = absolute_time_diff_us(last_tick_time_, now);
    }
    last_tick_time_ = now;
    break;
  }
  case SpeedModifier::DOUBLE_SPEED: {
    // Pass-through incoming tick immediately
    notify_observers(event);

    // Measure interval to schedule a mid tick for the NEXT gap
    if (!is_nil_time(last_tick_time_)) {
      last_interval_us_ = absolute_time_diff_us(last_tick_time_, now);
    }
    last_tick_time_ = now;

    // If we have a measured interval, schedule an inserted mid tick
    schedule_double_insert_after(now);
    break;
  }
  }
}

void SpeedAdapter::schedule_double_insert_after(absolute_time_t now) {
  if (last_interval_us_ == 0) {
    next_insert_time_ = nil_time;
    return;
  }
  // Schedule halfway into the last measured interval
  uint32_t half_us = last_interval_us_ / 2u;
  next_insert_time_ = delayed_by_us(now, static_cast<uint64_t>(half_us));
}

void SpeedAdapter::update(absolute_time_t /*now*/) {
  if (modifier_ != SpeedModifier::DOUBLE_SPEED) {
    return;
  }
  if (is_nil_time(next_insert_time_)) {
    return;
  }
  if (time_reached(next_insert_time_)) {
    // Emit an interpolated tick
    ClockEvent interp{current_source_};
    interp.is_resync = false;
    interp.is_physical_pulse = false;
    notify_observers(interp);
    // Clear until next source tick schedules another insert
    next_insert_time_ = nil_time;
  }
}

} // namespace musin::timing
