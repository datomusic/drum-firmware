#include "midi_test_support.h"
#include "musin/hal/null_logger.h"
#include "pico/time.h"
#include "test_support.h"

#include "musin/midi/midi_output_queue.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
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

TEST_CASE("TempoHandler internal clock emits tempo events and MIDI clock") {
  reset_test_state();

  InternalClock internal_clock(120.0f); // 120 BPM
  MidiClockProcessor midi_proc;
  // Pins are irrelevant in test; overridden SyncIn has no hardware
  musin::timing::SyncIn sync_in(0, 0);
  musin::timing::ClockMultiplier clk_mult(24);

  // Send MIDI clock even when stopped for this test to simplify assertion
  // Intentionally construct with a different initial source, then switch to
  // INTERNAL so set_clock_source performs full attach/start work.
  TempoHandler th(internal_clock, midi_proc, sync_in, clk_mult,
                  /*send_midi_clock_when_stopped*/ true, ClockSource::MIDI);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Ensure internal source is fully initialized (constructor does not attach)
  th.set_clock_source(ClockSource::INTERNAL);

  // InternalClock starts in set_clock_source; tick interval at 120 BPM is
  // ~20833us per 24ppqn
  constexpr uint64_t tick_us = 20833;

  // Generate 3 internal ticks
  for (int i = 0; i < 3; ++i) {
    internal_clock.update(get_absolute_time());
    // InternalClock schedules next tick, so advance time past interval
    advance_time_us(tick_us + 10);
    internal_clock.update(get_absolute_time());
  }

  // Flush MIDI queue to collect all realtime clocks
  musin::NullLogger logger;
  while (!musin::midi::midi_output_queue.empty()) {
    musin::midi::process_midi_output_queue(logger);
  }

  REQUIRE(rec.events.size() >= 3);
  // First few phases should advance by 1 per tick (starting from 0 -> 1,2,3)
  REQUIRE(rec.events[0].phase_24 == 1);
  REQUIRE(rec.events[1].phase_24 == 2);
  REQUIRE(rec.events[2].phase_24 == 3);

  // Expect at least 3 MIDI realtime Clock messages
  size_t rt_count = 0;
  for (const auto &call : mock_midi_calls) {
    if (call.function_name == std::string("_sendRealTime_actual") &&
        call.rt_type == ::midi::Clock) {
      ++rt_count;
    }
  }
  REQUIRE(rt_count >= 3);
}

TEST_CASE(
    "TempoHandler external sync half-speed anchors on every second pulse") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 0);
  musin::timing::ClockMultiplier clk_mult(24);

  // Construct with INTERNAL, then switch to EXTERNAL_SYNC to ensure full setup
  TempoHandler th(internal_clock, midi_proc, sync_in, clk_mult,
                  /*send_midi_clock_when_stopped*/ false,
                  ClockSource::INTERNAL);

  th.set_speed_modifier(SpeedModifier::HALF_SPEED);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Ensure external source is fully initialized (attach observers)
  th.set_clock_source(ClockSource::EXTERNAL_SYNC);

  // Simulate 4 external physical pulses (24ppqn multiplied elsewhere),
  // delivered as ClockEvents directly to TempoHandler.
  for (int i = 0; i < 4; ++i) {
    ClockEvent e{ClockSource::EXTERNAL_SYNC};
    e.is_physical_pulse = true;
    th.notification(e);
  }

  // With HALF speed, first pulse advances (no anchor), second pulse anchors to
  // 12, third advances, fourth anchors to 0. Verify those anchored phases.
  REQUIRE(rec.events.size() >= 4);
  REQUIRE(rec.events[1].phase_24 == musin::timing::PHASE_EIGHTH_OFFBEAT);
  REQUIRE(rec.events[3].phase_24 == musin::timing::PHASE_DOWNBEAT);
}

TEST_CASE(
    "TempoHandler manual sync in MIDI defers to next tick and flags resync") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 0);
  musin::timing::ClockMultiplier clk_mult(24);

  TempoHandler th(internal_clock, midi_proc, sync_in, clk_mult,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Ensure MIDI source is fully initialized (attach observers)
  th.set_clock_source(ClockSource::MIDI);

  // No recent MIDI tick -> manual sync should defer
  th.trigger_manual_sync();
  REQUIRE(rec.events.empty());

  // Next MIDI tick should be anchored and marked as resync
  ClockEvent e{ClockSource::MIDI};
  e.is_physical_pulse = false;
  th.notification(e);

  REQUIRE(rec.events.size() >= 1);
  REQUIRE(rec.events[0].is_resync == true);
  REQUIRE(rec.events[0].phase_24 == musin::timing::PHASE_DOWNBEAT);
}
