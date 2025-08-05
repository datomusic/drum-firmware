#ifndef MUSIN_TIMING_CLOCK_MULTIPLIER_H
#define MUSIN_TIMING_CLOCK_MULTIPLIER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h"
#include <cstdint>

namespace musin::timing {

constexpr size_t MAX_CLOCK_MULTIPLIER_OBSERVERS =
    2; // TempoHandler and potentially others

class ClockMultiplier
    : public etl::observer<musin::timing::ClockEvent>,
      public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_CLOCK_MULTIPLIER_OBSERVERS> {
public:
  explicit ClockMultiplier(uint8_t multiplication_factor);

  // This is the notification method for the etl::observer pattern.
  void notification(musin::timing::ClockEvent event);

  void update(absolute_time_t now);
  void reset();

private:
  uint8_t multiplication_factor_;
  uint8_t pulse_counter_ = 0;
  uint64_t pulse_interval_us_ = 0;
  absolute_time_t last_pulse_time_ = nil_time;
  absolute_time_t next_pulse_time_ = nil_time;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_CLOCK_MULTIPLIER_H