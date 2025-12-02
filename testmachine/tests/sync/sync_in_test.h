#ifndef TESTMACHINE_TESTS_SYNC_IN_TEST_H
#define TESTMACHINE_TESTS_SYNC_IN_TEST_H

#include "testmachine/test_framework/test_interface.h"

#include <cstdint>

namespace testmachine {

class SyncInTest : public Test {
public:
  static constexpr uint32_t DEFAULT_TIMEOUT_MS = 10000;
  static constexpr uint32_t MIN_PULSES_FOR_BPM = 4;
  static constexpr uint32_t DEBOUNCE_US = 5000;

  explicit SyncInTest(uint32_t sync_pin);

  const char *get_name() const override { return "SYNC_IN"; }

  void start(absolute_time_t now) override;
  void update(absolute_time_t now) override;
  bool is_complete() const override;
  TestResult get_result() const override;
  void reset() override;

  void set_timeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

private:
  uint32_t sync_pin_;
  uint32_t timeout_ms_ = DEFAULT_TIMEOUT_MS;

  uint32_t pulses_detected_ = 0;
  absolute_time_t first_pulse_time_;
  absolute_time_t last_pulse_time_;
  absolute_time_t timeout_time_;
  absolute_time_t debounce_end_time_;

  bool last_pin_state_ = false;
  bool waiting_for_low_ = false;

  bool complete_ = false;
  TestResult result_;
};

} // namespace testmachine

#endif // TESTMACHINE_TESTS_SYNC_IN_TEST_H
