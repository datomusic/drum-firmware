
#include "midi_test_support.h"
#include "musin/hal/null_logger.h"
#include "pico/time.h"
#include "test_support.h"

#include "musin/midi/midi_output_queue.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_out.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/speed_adapter.h"
#include "musin/timing/sync_in.h" // Test override provides stub
#include "musin/timing/tempo_handler.h"
#include "musin/timing/timing_constants.h"

#include <etl/observer.h>
#include <vector>

using musin::timing::ClockEvent;
using musin::timing::ClockSource;
using musin::timing::InternalClock;
using musin::timing::MidiClockProcessor;
using musin::timing::PlaybackState;
using musin::timing::SpeedModifier;
using musin::timing::TempoEvent;
using musin::timing::TempoHandler;

namespace {

// Simple observer to capture TempoEvents for assertions
struct TempoEventRecorder : etl::observer<TempoEvent> {
  std::vector<TempoEvent> events;
  void notification(TempoEvent e) {
    events.push_back(e);
  }
  void clear() {
    events.clear();
  }
};

// Helper to advance mock time
static void advance_time_us(uint64_t us) {
  advance_mock_time_us(us);
}

} // namespace

TEST_CASE("TempoHandler external sync half-speed forwards every other tick") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1);
  musin::timing::ClockMultiplier clock_multiplier(12);
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::EXTERNAL_SYNC);
  musin::timing::SpeedAdapter speed_adapter;

  TempoHandler th(
      internal_clock, midi_proc, sync_in, clock_router, speed_adapter,
      /*send_midi_clock_when_stopped*/ false, ClockSource::EXTERNAL_SYNC);

  th.set_speed_modifier(SpeedModifier::HALF_SPEED);

  clock_router.add_observer(clock_multiplier);
  clock_multiplier.add_observer(speed_adapter);
  speed_adapter.add_observer(th);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Simulate 4 external physical pulses.
  for (int i = 0; i < 4; ++i) {
    ClockEvent e{ClockSource::EXTERNAL_SYNC};
    e.is_physical_pulse = true;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    clock_router.notification(e);
  }

  REQUIRE(rec.events.size() == 2);
  REQUIRE(rec.events[0].phase_24 == 1);
  REQUIRE(rec.events[1].phase_24 == 2);
}

TEST_CASE("TempoHandler external sync double-speed interpolates correctly") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1);
  musin::timing::ClockMultiplier clock_multiplier(12);
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::EXTERNAL_SYNC);
  musin::timing::SpeedAdapter speed_adapter;

  TempoHandler th(
      internal_clock, midi_proc, sync_in, clock_router, speed_adapter,
      /*send_midi_clock_when_stopped*/ false, ClockSource::EXTERNAL_SYNC);

  th.set_speed_modifier(SpeedModifier::DOUBLE_SPEED);

  clock_router.add_observer(clock_multiplier);
  clock_multiplier.add_observer(speed_adapter);
  speed_adapter.add_observer(th);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Simulate 2 physical pulses.
  for (int i = 0; i < 2; ++i) {
    ClockEvent e{ClockSource::EXTERNAL_SYNC};
    e.is_physical_pulse = true;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    clock_router.notification(e);

    // Simulate time passing for interpolated ticks to be generated
    for (int j = 0; j < 12; ++j) {
      advance_time_us(1000);
      clock_multiplier.update(get_absolute_time());
      speed_adapter.update(get_absolute_time());
    }
  }

  REQUIRE(rec.events.size() >= 40);

  bool phase_always_increments = true;
  for (size_t i = 1; i < rec.events.size(); ++i) {
    uint8_t expected_phase = (rec.events[i - 1].phase_24 + 1) % 24;
    if (rec.events[i].phase_24 != expected_phase) {
      phase_always_increments = false;
      break;
    }
  }
  REQUIRE(phase_always_increments);
}
