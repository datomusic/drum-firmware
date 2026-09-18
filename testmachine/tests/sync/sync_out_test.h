#ifndef TESTMACHINE_TESTS_SYNC_OUT_TEST_H
#define TESTMACHINE_TESTS_SYNC_OUT_TEST_H

#include "testmachine/test_framework/test_interface.h"

#include <cstdint>

namespace testmachine {

class SyncOutTest : public Test {
public:
  static constexpr uint32_t DEFAULT_NUM_PULSES = 10;
  static constexpr uint32_t DEFAULT_INTERVAL_MS = 500;
  static constexpr uint32_t PULSE_DURATION_MS = 10;

  explicit SyncOutTest(uint32_t sync_pin);

  const char *get_name() const override { return "SYNC_OUT"; }

  void start(absolute_time_t now) override;
  void update(absolute_time_t now) override;
  bool is_complete() const override;
  TestResult get_result() const override;
  void reset() override;

  void set_num_pulses(uint32_t pulses) { target_pulses_ = pulses; }
  void set_interval(uint32_t interval_ms) { interval_ms_ = interval_ms; }

private:
  uint32_t sync_pin_;
  uint32_t target_pulses_ = DEFAULT_NUM_PULSES;
  uint32_t interval_ms_ = DEFAULT_INTERVAL_MS;

  uint32_t pulses_sent_ = 0;
  absolute_time_t next_pulse_time_;
  absolute_time_t pulse_off_time_;
  bool pulse_active_ = false;

  bool complete_ = false;
  TestResult result_;
};

} // namespace testmachine

#endif // TESTMACHINE_TESTS_SYNC_OUT_TEST_H
