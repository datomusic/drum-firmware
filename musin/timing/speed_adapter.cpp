#include "musin/timing/speed_adapter.h"

namespace musin::timing {

void SpeedAdapter::notification(musin::timing::ClockEvent event) {
  current_source_ = event.source;

  if (event.is_resync) {
    // Forward resync immediately and reset internal state
    notify_observers(event);
    last_tick_us_ = 0;
    last_interval_us_ = 0;
    next_insert_time_ = nil_time;
    return;
  }

  // Use provided timestamp from the source
  uint32_t now_us = event.timestamp_us;

  switch (modifier_) {
  case SpeedModifier::NORMAL_SPEED:
  case SpeedModifier::HALF_SPEED: {
    // HALF_SPEED is now handled by individual clock sources,
    // so just pass through like NORMAL_SPEED
    notify_observers(event);
    // Track interval for potential future DOUBLE transitions
    if (last_tick_us_ != 0) {
      last_interval_us_ = now_us - last_tick_us_;
    }
    last_tick_us_ = now_us;
    break;
  }
  case SpeedModifier::DOUBLE_SPEED: {
    // Pass-through incoming tick immediately
    notify_observers(event);

    // Measure interval to schedule a mid tick for the NEXT gap
    if (last_tick_us_ != 0) {
      last_interval_us_ = now_us - last_tick_us_;
    }
    last_tick_us_ = now_us;

    // If we have a measured interval, schedule an inserted mid tick
    schedule_double_insert_after(get_absolute_time());
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
    interp.is_downbeat = false;
    interp.anchor_to_phase = ClockEvent::ANCHOR_PHASE_NONE;
    interp.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    notify_observers(interp);
    // Clear until next source tick schedules another insert
    next_insert_time_ = nil_time;
  }
}

} // namespace musin::timing
