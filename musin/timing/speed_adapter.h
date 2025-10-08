#ifndef MUSIN_TIMING_SPEED_ADAPTER_H
#define MUSIN_TIMING_SPEED_ADAPTER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h"

namespace musin::timing {

constexpr size_t MAX_SPEED_ADAPTER_OBSERVERS = 2;

/**
 * Downsamples raw 24 PPQN clock sources to variable-rate output for
 * downstream 12-PPQN consumers.
 *
 * - NORMAL: emit every 2nd tick (24→12 PPQN).
 * - HALF_SPEED: emit every 4th tick (24→6 PPQN).
 * - DOUBLE: pass through all ticks (24 PPQN output, phase wraps 0→11
 * twice/quarter).
 *
 * Resets divider on incoming resync. DOUBLE mode ticks maintain physical flag.
 */
class SpeedAdapter
    : public etl::observer<musin::timing::ClockEvent>,
      public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_SPEED_ADAPTER_OBSERVERS> {
public:
  explicit SpeedAdapter(SpeedModifier initial = SpeedModifier::NORMAL_SPEED)
      : modifier_(initial) {
  }

  void set_modifier(SpeedModifier m) {
    if (modifier_ == m && !has_pending_modifier_)
      return;

    // For EXTERNAL_SYNC, defer the modifier change until next downbeat
    if (current_source_ == ClockSource::EXTERNAL_SYNC) {
      pending_modifier_ = m;
      has_pending_modifier_ = true;
    } else {
      // For INTERNAL/MIDI, apply immediately
      modifier_ = m;
      tick_counter_ = 0;
      has_pending_modifier_ = false;
    }
  }

  [[nodiscard]] SpeedModifier get_modifier() const {
    return modifier_;
  }

  void notification(musin::timing::ClockEvent event) override;

private:
  SpeedModifier modifier_;
  uint32_t tick_counter_ = 0;
  ClockSource current_source_ = ClockSource::INTERNAL;
  SpeedModifier pending_modifier_ = SpeedModifier::NORMAL_SPEED;
  bool has_pending_modifier_ = false;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SPEED_ADAPTER_H
