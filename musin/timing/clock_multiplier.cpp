#include "musin/timing/clock_multiplier.h"
#include "musin/timing/timing_constants.h"
#include "pico/time.h"
#include <cassert>

namespace musin::timing {

ClockMultiplier::ClockMultiplier(uint8_t multiplication_factor)
    : base_multiplication_factor_(multiplication_factor) {
  assert(multiplication_factor > 0 &&
         "ClockMultiplier: multiplication_factor cannot be zero - would cause "
         "division by zero");
  reset();
}

void ClockMultiplier::notification(musin::timing::ClockEvent event) {
  absolute_time_t now = get_absolute_time();
  bool send_resync = false;
  bool is_physical = event.is_physical_pulse;

  // Store the current source from the incoming event
  current_source_ = event.source;

  if (!is_nil_time(last_pulse_time_)) {
    uint64_t diff = absolute_time_diff_us(last_pulse_time_, now);
    // Always calculate interval for consistent 24 PPQN output
    // Speed modification is handled by TempoHandler for phase consistency
    pulse_interval_us_ = diff / base_multiplication_factor_;
  } else {
    // First pulse after reset/startup - send resync for initial alignment
    send_resync = true;
  }

  last_pulse_time_ = now;
  pulse_counter_ = 0;

  // Send the first pulse immediately, preserving the original source and
  // whether this was a physical pulse
  ClockEvent multiplied_event{current_source_, send_resync};
  multiplied_event.is_physical_pulse = is_physical;
  // Propagate timestamp if provided, else approximate with now
  multiplied_event.timestamp_us =
      (event.timestamp_us != 0) ? event.timestamp_us
                                : static_cast<uint32_t>(to_us_since_boot(now));
  multiplied_event.anchor_to_phase = ClockEvent::ANCHOR_PHASE_NONE;
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
    ClockEvent interp_event{current_source_};
    // Interpolated ticks are not physical pulses
    interp_event.is_physical_pulse = false;
    // Interpolated ticks must not carry anchoring intent
    interp_event.anchor_to_phase = ClockEvent::ANCHOR_PHASE_NONE;
    // Stamp with scheduled time for this interpolated pulse
    interp_event.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(next_pulse_time_));
    notify_observers(interp_event);
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

// Note: Speed modification is handled by TempoHandler; no speed controls here.

} // namespace musin::timing
