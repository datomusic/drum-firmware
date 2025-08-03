#ifndef MUSIN_HAL_INTERNAL_CLOCK_H
#define MUSIN_HAL_INTERNAL_CLOCK_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/timing_constants.h"
#include <atomic>
#include <cstdint>

extern "C" {
#include "pico/time.h"
}

namespace musin::timing {

constexpr size_t MAX_CLOCK_OBSERVERS = 3;

class InternalClock
    : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_CLOCK_OBSERVERS> {
public:
  static constexpr uint32_t PPQN = musin::timing::DEFAULT_PPQN;

  explicit InternalClock(float initial_bpm = 120.0f);

  InternalClock(const InternalClock &) = delete;
  InternalClock &operator=(const InternalClock &) = delete;

  void set_bpm(float bpm);
  [[nodiscard]] float get_bpm() const;
  [[nodiscard]] bool is_running() const;

  void set_discipline(ClockSource source, uint32_t ppqn);
  void reference_tick_received(absolute_time_t now, ClockSource source);

private:
  static bool timer_callback(struct repeating_timer *rt);
  int64_t calculate_tick_interval_us(float bpm) const;

  // PLL state variables
  float target_bpm_;
  float current_bpm_;
  float phase_error_us_ = 0.0f;
  float phase_error_integral_ = 0.0f;
  absolute_time_t last_internal_tick_time_ = nil_time;
  absolute_time_t last_reference_tick_time_ = nil_time;
  ClockSource discipline_source_ = ClockSource::INTERNAL;
  uint32_t discipline_ppqn_ = DEFAULT_PPQN;

  // Timer-related variables
  struct repeating_timer timer_info_;
  std::atomic<bool> is_running_{false};
};

} // namespace musin::timing

#endif // MUSIN_HAL_INTERNAL_CLOCK_H