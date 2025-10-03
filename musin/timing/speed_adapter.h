#ifndef MUSIN_TIMING_SPEED_ADAPTER_H
#define MUSIN_TIMING_SPEED_ADAPTER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h"

namespace musin::timing {

constexpr size_t MAX_SPEED_ADAPTER_OBSERVERS = 2;

/**
 * Inserts a reusable speed-scaling stage between raw 24 PPQN clock sources
 * and downstream consumers.
 *
 * - NORMAL: pass-through.
 * - HALF_SPEED: emit every 2nd tick.
 * - DOUBLE: pass through incoming ticks and insert an interpolated tick midway
 *   between them using the previous measured interval.
 *
 * Resets on incoming resync. Interpolated ticks are marked as
 * is_downbeat=false.
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
    if (modifier_ == m)
      return;
    modifier_ = m;
    tick_counter_ = 0;
    last_tick_us_ = 0;
    last_interval_us_ = 0;
    next_insert_time_ = nil_time;
  }

  [[nodiscard]] SpeedModifier get_modifier() const {
    return modifier_;
  }

  void notification(musin::timing::ClockEvent event) override;

  void update(absolute_time_t now);

private:
  void schedule_double_insert_after(absolute_time_t now);

  SpeedModifier modifier_;
  uint32_t tick_counter_ = 0;

  // Timing for DOUBLE mode
  uint32_t last_tick_us_ = 0;     // timestamp of last source tick
  uint32_t last_interval_us_ = 0; // measured between source ticks
  absolute_time_t next_insert_time_ = nil_time;
  ClockSource current_source_ = ClockSource::INTERNAL;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SPEED_ADAPTER_H
