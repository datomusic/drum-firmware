#include "musin/timing/speed_adapter.h"

namespace musin::timing {

void SpeedAdapter::notification(musin::timing::ClockEvent event) {
  current_source_ = event.source;

  if (event.is_resync) {
    notify_observers(event);
    tick_counter_ = 0;
    last_tick_us_ = 0;
    last_interval_us_ = 0;
    next_insert_time_ = nil_time;
    return;
  }

  uint32_t now_us = event.timestamp_us;
  tick_counter_++;

  switch (modifier_) {
  case SpeedModifier::HALF_SPEED:
    // Emit every 4th tick: 24→6 PPQN
    if (tick_counter_ % 4 == 0) {
      notify_observers(event);
    }
    if (last_tick_us_ != 0) {
      last_interval_us_ = now_us - last_tick_us_;
    }
    last_tick_us_ = now_us;
    break;

  case SpeedModifier::NORMAL_SPEED:
    // Emit every 2nd tick: 24→12 PPQN
    if (tick_counter_ % 2 == 0) {
      notify_observers(event);
    }
    if (last_tick_us_ != 0) {
      last_interval_us_ = now_us - last_tick_us_;
    }
    last_tick_us_ = now_us;
    break;

  case SpeedModifier::DOUBLE_SPEED:
    // Pass through all ticks: 24 PPQN (phase wraps 0→11 twice per quarter)
    notify_observers(event);
    if (last_tick_us_ != 0) {
      last_interval_us_ = now_us - last_tick_us_;
    }
    last_tick_us_ = now_us;
    break;
  }
}

void SpeedAdapter::schedule_double_insert_after(absolute_time_t /*now*/) {
  // No longer used - DOUBLE mode now passes through all ticks
}

void SpeedAdapter::update(absolute_time_t /*now*/) {
  // No longer used - DOUBLE mode no longer inserts interpolated ticks
}

} // namespace musin::timing
