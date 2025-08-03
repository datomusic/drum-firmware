#include "internal_clock.h"

#include "musin/hal/debug_utils.h"
#include "musin/timing/clock_event.h"
#include "pico/stdlib.h"
#include <algorithm>
#include <cstdio>

namespace musin::timing {

/**
 * @brief Proportional-Integral (PI) controller tuning constants for the PLL.
 *
 * These values determine how the clock adjusts to phase errors.
 * - KP (Proportional Gain): Reacts to the current phase error. A higher value
 *   makes the clock correct faster but can lead to overshoot and instability.
 * - KI (Integral Gain): Reacts to the accumulated phase error over time. It
 *   corrects for small, steady-state frequency drifts but a high value can
 *   cause oscillations.
 * - INTEGRAL_WINDUP_LIMIT: Prevents the integral term from growing too large,
 *   which can cause the clock to take a very long time to recover from a large
 *   frequency change.
 */
constexpr float KP = 0.0001f;
constexpr float KI = 0.00000f;
constexpr float INTEGRAL_WINDUP_LIMIT = 50000.0f;

InternalClock::InternalClock(float initial_bpm)
    : target_bpm_(initial_bpm), current_bpm_(initial_bpm) {
  timer_info_ = {};
  int64_t tick_interval_us = calculate_tick_interval_us(current_bpm_);

  if (tick_interval_us > 0) {
    if (add_repeating_timer_us(-tick_interval_us, timer_callback, this,
                               &timer_info_)) {
      is_running_.store(true, std::memory_order_relaxed);
      last_internal_tick_time_ = get_absolute_time();
    }
  }
}

void InternalClock::set_bpm(float bpm) {
  if (bpm <= 0.0f) {
    return;
  }
  target_bpm_ = bpm;

  if (discipline_source_ == ClockSource::INTERNAL) {
    current_bpm_ = target_bpm_;
    phase_error_integral_ = 0.0f; // Reset integral when manually setting
  }
}

float InternalClock::get_bpm() const {
  return current_bpm_;
}

bool InternalClock::is_running() const {
  return is_running_.load(std::memory_order_relaxed);
}

void InternalClock::set_discipline(ClockSource source, uint32_t ppqn) {
  discipline_source_ = source;
  discipline_ppqn_ = ppqn;

  // Reset PLL state when changing source to ensure a clean start
  phase_error_us_ = 0.0f;
  phase_error_integral_ = 0.0f;
  last_reference_tick_time_ = nil_time;

  // If switching back to internal, immediately snap to the target BPM
  if (source == ClockSource::INTERNAL) {
    current_bpm_ = target_bpm_;
  }
}

void InternalClock::reference_tick_received(absolute_time_t now,
                                            ClockSource source) {
  if (source != discipline_source_ || is_nil_time(now)) {
    return;
  }

  if (is_nil_time(last_reference_tick_time_)) {
    // First tick from this source, just record the time and wait for the next
    // one.
    last_reference_tick_time_ = now;
    return;
  }

  // 1. Calculate the expected interval of the reference clock based on our
  // current BPM.
  float ref_ticks_per_second =
      (current_bpm_ / 60.0f) * static_cast<float>(discipline_ppqn_);
  if (ref_ticks_per_second <= 0) {
    return;
  }
  int64_t expected_ref_interval_us =
      static_cast<int64_t>(1000000.0f / ref_ticks_per_second);

  // 2. Calculate the actual time since the last reference tick.
  int64_t actual_ref_interval_us =
      absolute_time_diff_us(last_reference_tick_time_, now);

  // 3. Calculate phase error.
  phase_error_us_ =
      static_cast<float>(expected_ref_interval_us - actual_ref_interval_us);

  // 4. Update integral term with anti-windup.
  phase_error_integral_ += phase_error_us_;
  phase_error_integral_ = std::clamp(
      phase_error_integral_, -INTEGRAL_WINDUP_LIMIT, INTEGRAL_WINDUP_LIMIT);

  // 5. Apply PI controller to get a BPM adjustment.
  float adjustment = (KP * phase_error_us_) + (KI * phase_error_integral_);

  // 6. Update the clock's frequency.
  current_bpm_ = target_bpm_ + adjustment;

  // 7. Store the time of this tick for the next calculation.
  last_reference_tick_time_ = now;

#ifdef VERBOSE
  DEBUG_PRINT("Phase Error: %.2f us, Integral: %.2f, Adj: %.4f, "
              "Current BPM: %.2f\n",
              phase_error_us_, phase_error_integral_, adjustment, current_bpm_);
#endif
}

int64_t InternalClock::calculate_tick_interval_us(float bpm) const {
  if (bpm <= 0.0f) {
    return 0;
  }
  float ticks_per_second = (bpm / 60.0f) * static_cast<float>(PPQN);
  if (ticks_per_second <= 0.0f) {
    return 0;
  }
  return static_cast<int64_t>(1000000.0f / ticks_per_second);
}

bool InternalClock::timer_callback(struct repeating_timer *rt) {
  InternalClock *instance = static_cast<InternalClock *>(rt->user_data);

  instance->last_internal_tick_time_ = get_absolute_time();

  musin::timing::ClockEvent tick_event{musin::timing::ClockSource::INTERNAL};
  instance->notify_observers(tick_event);

  int64_t next_interval_us =
      instance->calculate_tick_interval_us(instance->current_bpm_);

  if (next_interval_us <= 0) {
    instance->is_running_.store(false, std::memory_order_relaxed);
    return false;
  }

  rt->delay_us = -next_interval_us;

  return true;
}

} // namespace musin::timing
