#include "sync_in_test.h"

extern "C" {
#include "hardware/gpio.h"
}

#include <cstdio>

namespace testmachine {

SyncInTest::SyncInTest(uint32_t sync_pin) : sync_pin_(sync_pin) {
  gpio_init(sync_pin_);
  gpio_set_dir(sync_pin_, GPIO_IN);
  gpio_pull_down(sync_pin_);
}

void SyncInTest::start(absolute_time_t now) {
  complete_ = false;
  pulses_detected_ = 0;
  first_pulse_time_ = nil_time;
  last_pulse_time_ = nil_time;
  debounce_end_time_ = nil_time;
  last_pin_state_ = false;
  waiting_for_low_ = false;
  result_ = {TestStatus::Running, ""};

  timeout_time_ = delayed_by_ms(now, timeout_ms_);
}

void SyncInTest::update(absolute_time_t now) {
  if (complete_) {
    return;
  }

  if (time_reached(timeout_time_)) {
    complete_ = true;
    if (pulses_detected_ < 2) {
      result_ = TestResult::timeout("no pulses detected");
    } else {
      uint64_t interval_us =
          absolute_time_diff_us(first_pulse_time_, last_pulse_time_);
      uint64_t avg_interval_us = interval_us / (pulses_detected_ - 1);
      uint32_t bpm = static_cast<uint32_t>(60000000ULL / (avg_interval_us * 2));

      char msg[64];
      snprintf(msg, sizeof(msg), "timeout: %lu pulses, ~%lu BPM", pulses_detected_,
               bpm);
      result_ = TestResult::timeout(msg);
    }
    return;
  }

  bool current_state = gpio_get(sync_pin_);

  if (waiting_for_low_) {
    if (!current_state && time_reached(debounce_end_time_)) {
      waiting_for_low_ = false;
    }
  } else {
    if (current_state && !last_pin_state_) {
      if (pulses_detected_ == 0) {
        first_pulse_time_ = now;
      }
      last_pulse_time_ = now;
      ++pulses_detected_;

      waiting_for_low_ = true;
      debounce_end_time_ = delayed_by_us(now, DEBOUNCE_US);
    }
  }

  last_pin_state_ = current_state;

  if (pulses_detected_ >= MIN_PULSES_FOR_BPM) {
    complete_ = true;
    uint64_t interval_us =
        absolute_time_diff_us(first_pulse_time_, last_pulse_time_);
    uint64_t avg_interval_us = interval_us / (pulses_detected_ - 1);
    uint32_t bpm = static_cast<uint32_t>(60000000ULL / (avg_interval_us * 2));

    char msg[64];
    snprintf(msg, sizeof(msg), "%lu pulses, ~%lu BPM", pulses_detected_, bpm);
    result_ = TestResult::passed(msg);
  }
}

bool SyncInTest::is_complete() const { return complete_; }

TestResult SyncInTest::get_result() const { return result_; }

void SyncInTest::reset() {
  complete_ = false;
  pulses_detected_ = 0;
  first_pulse_time_ = nil_time;
  last_pulse_time_ = nil_time;
  last_pin_state_ = false;
  waiting_for_low_ = false;
  result_ = {TestStatus::NotStarted, ""};
}

} // namespace testmachine
