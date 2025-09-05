#include "musin/timing/clock_multiplier.h"
#include "pico/time.h"

namespace musin::timing {

ClockMultiplier::ClockMultiplier(uint8_t multiplication_factor)
    : base_multiplication_factor_(multiplication_factor) {
  reset();
}

void ClockMultiplier::notification(musin::timing::ClockEvent event) {
  absolute_time_t now = get_absolute_time();

  if (!is_nil_time(last_pulse_time_)) {
    uint64_t diff = absolute_time_diff_us(last_pulse_time_, now);
    // Always calculate interval for consistent 24 PPQN output
    // Speed modification is handled by TempoHandler for phase consistency
    pulse_interval_us_ = diff / base_multiplication_factor_;
  }

  last_pulse_time_ = now;
  pulse_counter_ = 0;

  // Send the first pulse immediately with resync flag for step alignment
  ClockEvent multiplied_event{ClockSource::EXTERNAL_SYNC, true};
  notify_observers(multiplied_event);
  pulse_counter_++;
  if (pulse_interval_us_ > 0) {
    next_pulse_time_ = delayed_by_us(last_pulse_time_, pulse_interval_us_);
  } else {
    next_pulse_time_ = nil_time;
  }
}

void ClockMultiplier::update(absolute_time_t now) {
  if (pulse_counter_ == 0 || pulse_counter_ >= base_multiplication_factor_ ||
      is_nil_time(next_pulse_time_)) {
    return;
  }

  if (time_reached(next_pulse_time_)) {
    ClockEvent event{ClockSource::EXTERNAL_SYNC};
    notify_observers(event);
    pulse_counter_++;
    next_pulse_time_ = delayed_by_us(next_pulse_time_, pulse_interval_us_);
  }
}

void ClockMultiplier::reset() {
  pulse_counter_ = 0;
  pulse_interval_us_ = 0;
  last_pulse_time_ = nil_time;
  next_pulse_time_ = nil_time;
}

void ClockMultiplier::set_speed_modifier(
    [[maybe_unused]] SpeedModifier modifier) {
  // Speed modification is now handled uniformly by TempoHandler
  // This method is retained for interface compatibility but does nothing
}

} // namespace musin::timing