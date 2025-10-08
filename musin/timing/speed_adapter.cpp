#include "musin/timing/speed_adapter.h"

namespace musin::timing {

void SpeedAdapter::notification(musin::timing::ClockEvent event) {
  current_source_ = event.source;

  if (event.is_resync) {
    notify_observers(event);
    tick_counter_ = 0;
    return;
  }

  if (event.is_beat) {
    // Apply pending modifier on downbeat for external sync alignment
    if (has_pending_modifier_) {
      modifier_ = pending_modifier_;
      has_pending_modifier_ = false;
    }
    notify_observers(event);
    tick_counter_ = 0;
    return;
  }

  tick_counter_++;

  switch (modifier_) {
  case SpeedModifier::HALF_SPEED:
    // Emit every 4th tick: 24→6 PPQN
    if (tick_counter_ % 4 == 0) {
      notify_observers(event);
    }
    break;

  case SpeedModifier::NORMAL_SPEED:
    // Emit every 2nd tick: 24→12 PPQN
    if (tick_counter_ % 2 == 0) {
      notify_observers(event);
    }
    break;

  case SpeedModifier::DOUBLE_SPEED:
    // Pass through all ticks: 24 PPQN (phase wraps 0→11 twice per quarter)
    notify_observers(event);
    break;
  }
}

} // namespace musin::timing
