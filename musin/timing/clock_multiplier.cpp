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
    // Calculate the interval for 24 PPQN pulses based on the 4 PPQN input
    pulse_interval_us_ = diff / calculate_effective_multiplier();
  }

  last_pulse_time_ = now;
  pulse_counter_ = 0;

  // Send the first pulse immediately
  ClockEvent multiplied_event{ClockSource::EXTERNAL_SYNC};
  notify_observers(multiplied_event);
  pulse_counter_++;
  if (pulse_interval_us_ > 0) {
    next_pulse_time_ = delayed_by_us(last_pulse_time_, pulse_interval_us_);
  } else {
    next_pulse_time_ = nil_time;
  }
}

void ClockMultiplier::update(absolute_time_t now) {
  if (pulse_counter_ == 0 ||
      pulse_counter_ >= calculate_effective_multiplier() ||
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

void ClockMultiplier::set_speed_modifier(SpeedModifier modifier) {
  current_speed_modifier_ = modifier;
}

uint8_t ClockMultiplier::calculate_effective_multiplier() const {
  uint8_t ret = base_multiplication_factor_;
  switch (current_speed_modifier_) {
  case SpeedModifier::HALF_SPEED:
    ret /= 2;
    break;
  case SpeedModifier::DOUBLE_SPEED:
    ret *= 2;
    break;
  case SpeedModifier::NORMAL_SPEED:
  default:
    break;
  }
  return ret;
}

} // namespace musin::timing