#include "sync_out_test.h"

extern "C" {
#include "hardware/gpio.h"
}

#include <cstdio>

namespace testmachine {

SyncOutTest::SyncOutTest(uint32_t sync_pin) : sync_pin_(sync_pin) {
  gpio_init(sync_pin_);
  gpio_set_dir(sync_pin_, GPIO_OUT);
  gpio_put(sync_pin_, 0);
}

void SyncOutTest::start(absolute_time_t now) {
  complete_ = false;
  pulses_sent_ = 0;
  pulse_active_ = false;
  result_ = {TestStatus::Running, ""};

  next_pulse_time_ = now;
}

void SyncOutTest::update(absolute_time_t now) {
  if (complete_) {
    return;
  }

  if (pulse_active_ && time_reached(pulse_off_time_)) {
    gpio_put(sync_pin_, 0);
    pulse_active_ = false;
  }

  if (!pulse_active_ && pulses_sent_ < target_pulses_ &&
      time_reached(next_pulse_time_)) {
    gpio_put(sync_pin_, 1);
    pulse_active_ = true;
    pulse_off_time_ = delayed_by_ms(now, PULSE_DURATION_MS);
    next_pulse_time_ = delayed_by_ms(now, interval_ms_);
    ++pulses_sent_;
  }

  if (pulses_sent_ >= target_pulses_ && !pulse_active_) {
    complete_ = true;
    char msg[64];
    snprintf(msg, sizeof(msg), "sent %lu pulses at %lums interval",
             pulses_sent_, interval_ms_);
    result_ = TestResult::passed(msg);
  }
}

bool SyncOutTest::is_complete() const { return complete_; }

TestResult SyncOutTest::get_result() const { return result_; }

void SyncOutTest::reset() {
  complete_ = false;
  pulses_sent_ = 0;
  pulse_active_ = false;
  gpio_put(sync_pin_, 0);
  result_ = {TestStatus::NotStarted, ""};
}

} // namespace testmachine
