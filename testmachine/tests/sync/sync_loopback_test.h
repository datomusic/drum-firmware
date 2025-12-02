#ifndef TESTMACHINE_TESTS_SYNC_LOOPBACK_TEST_H
#define TESTMACHINE_TESTS_SYNC_LOOPBACK_TEST_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/sync_out.h"
#include "pico/time.h"
#include "testmachine/test_framework/test_interface.h"

#include <cstdint>

namespace testmachine {

/**
 * @brief Hardware loopback test for SYNC OUT -> SYNC IN path.
 *
 * Tests the complete sync signal chain by:
 * 1. Using SyncOut to generate pulses (feeds it clock events)
 * 2. Using SyncIn to detect pulses (observes its notifications)
 * 3. Verifying cable detection works (SYNC_DETECT pin)
 * 4. Counting received pulses match expected count
 */
class SyncLoopbackTest : public Test,
                         public etl::observer<musin::timing::ClockEvent> {
public:
  static constexpr uint32_t TICK_INTERVAL_MS = 20; // Send ticks every 20ms
  static constexpr uint32_t TICKS_PER_PULSE = 12;  // SyncOut default
  static constexpr uint32_t TARGET_PULSES = 10;    // Number of pulses to verify
  static constexpr uint32_t TIMEOUT_MS = 5000;     // 5 second timeout

  SyncLoopbackTest(musin::timing::SyncOut &sync_out,
                   musin::timing::SyncIn &sync_in);
  ~SyncLoopbackTest();

  const char *get_name() const override { return "SYNC_LOOPBACK"; }

  void start(absolute_time_t now) override;
  void update(absolute_time_t now) override;
  bool is_complete() const override;
  TestResult get_result() const override;
  void reset() override;

  // Observer interface - receives notifications from SyncIn when pulses detected
  void notification(musin::timing::ClockEvent event) override;

private:
  musin::timing::SyncOut &sync_out_;
  musin::timing::SyncIn &sync_in_;

  absolute_time_t next_tick_time_;
  absolute_time_t timeout_time_;

  uint32_t ticks_sent_ = 0;
  uint32_t target_ticks_ = TARGET_PULSES * TICKS_PER_PULSE;
  uint32_t pulses_received_ = 0;

  bool complete_ = false;
  TestResult result_;
};

} // namespace testmachine

#endif // TESTMACHINE_TESTS_SYNC_LOOPBACK_TEST_H
