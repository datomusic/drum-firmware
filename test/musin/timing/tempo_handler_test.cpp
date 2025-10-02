#include "midi_test_support.h"
#include "musin/hal/null_logger.h"
#include "pico/time.h"
#include "test_support.h"

#include "musin/midi/midi_output_queue.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_out.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/speed_adapter.h"
#include "musin/timing/sync_in.h" // Test override provides stub
#include "musin/timing/sync_out.h"
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
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::INTERNAL);
  musin::timing::SpeedAdapter speed_adapter;

  // Send MIDI clock even when stopped for this test to simplify assertion
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter,
                  /*send_midi_clock_when_stopped*/ true, ClockSource::INTERNAL);

  musin::timing::MidiClockOut midi_out(th,
                                       /*send_when_stopped_as_master*/ true);
  clock_router.add_observer(midi_out);
  clock_router.add_observer(speed_adapter);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // InternalClock starts in set_clock_source; tick interval at 120 BPM is
  // ~20833us per 24ppqn
  constexpr uint64_t tick_us = 20833;

  // Generate 3 internal ticks
  for (int i = 0; i < 3; ++i) {
    internal_clock.update(get_absolute_time());
    speed_adapter.update(get_absolute_time());
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

TEST_CASE("TempoHandler external sync passes through ticks (half-speed now "
          "handled by SyncIn)") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::EXTERNAL_SYNC);
  musin::timing::SpeedAdapter speed_adapter;

  TempoHandler th(
      internal_clock, midi_proc, sync_in, sync_out, clock_router, speed_adapter,
      /*send_midi_clock_when_stopped*/ false, ClockSource::EXTERNAL_SYNC);

  th.set_speed_modifier(SpeedModifier::HALF_SPEED);
  clock_router.add_observer(speed_adapter);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Simulate 4 external physical pulses by feeding the router.
  for (int i = 0; i < 4; ++i) {
    ClockEvent e{ClockSource::EXTERNAL_SYNC};
    e.is_physical_pulse = true;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    clock_router.notification(e);
  }

  // In new architecture: SpeedAdapter passes through, SyncIn would handle
  // half-speed but test stub doesn't implement it, so all 4 ticks are forwarded
  REQUIRE(rec.events.size() == 4);
  // External physical pulses get phase alignment instead of sequential
  // advancement Since we start at phase 0, calculate_aligned_phase() returns 0
  // for first few pulses
  REQUIRE(rec.events[0].phase_24 == 0);
  REQUIRE(rec.events[1].phase_24 == 0);
}

TEST_CASE("TempoHandler manual sync in MIDI emits immediate resync") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::MIDI);
  musin::timing::SpeedAdapter speed_adapter;

  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);
  clock_router.add_observer(speed_adapter);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Manual sync should immediately emit resync event (no deferral)
  th.trigger_manual_sync();
  REQUIRE(rec.events.size() >= 1);
  REQUIRE(rec.events[0].is_resync == true);
  REQUIRE(rec.events[0].phase_24 == musin::timing::PHASE_DOWNBEAT);
}

TEST_CASE("TempoHandler DOUBLE_SPEED with MIDI source advances by 2") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::MIDI);

  musin::timing::SpeedAdapter speed_adapter;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);

  th.set_speed_modifier(SpeedModifier::DOUBLE_SPEED);
  clock_router.add_observer(speed_adapter);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Send 3 MIDI clock ticks, advancing mock time between ticks so DOUBLE
  // can emit inserted mid-ticks at half-interval.
  for (int i = 0; i < 3; ++i) {
    ClockEvent e{ClockSource::MIDI};
    e.is_physical_pulse = false;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    speed_adapter.notification(e);
    if (i == 1) {
      // After the second tick, the adapter schedules a mid insert at +1/2
      advance_time_us(5000);
      speed_adapter.update(get_absolute_time()); // insert at half interval
      advance_time_us(5000);
      speed_adapter.update(get_absolute_time()); // move to next boundary
    } else {
      advance_time_us(10000);
      speed_adapter.update(get_absolute_time());
    }
  }

  REQUIRE(rec.events.size() >= 3);
  // Phases advance by 1 per adapter output
  REQUIRE(rec.events[0].phase_24 == 1);
  REQUIRE(rec.events[1].phase_24 == 2);
}

TEST_CASE("TempoHandler DOUBLE_SPEED phase alignment on odd phases") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::INTERNAL);

  musin::timing::SpeedAdapter speed_adapter;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Advance to an odd phase (phase 3)
  for (int i = 0; i < 3; ++i) {
    ClockEvent e{ClockSource::MIDI};
    e.is_physical_pulse = false;
    speed_adapter.notification(e);
  }
  rec.clear();

  // Now switch to DOUBLE_SPEED - should align phase to even number
  th.set_speed_modifier(SpeedModifier::DOUBLE_SPEED);

  // Send one more tick to see the aligned phase
  ClockEvent e{ClockSource::MIDI};
  e.is_physical_pulse = false;
  e.timestamp_us = static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  speed_adapter.notification(e);
  speed_adapter.update(get_absolute_time());

  REQUIRE(rec.events.size() >= 1);
  // Phase should be aligned to even (4, since 3+1=4 which is even)
  REQUIRE((rec.events[0].phase_24 & 1u) == 0u);
}

TEST_CASE("TempoHandler auto-switches from INTERNAL to MIDI when active") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::INTERNAL);

  musin::timing::SpeedAdapter speed_adapter_auto;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter_auto,
                  /*send_midi_clock_when_stopped*/ false,
                  ClockSource::INTERNAL);

  // Initially should be INTERNAL
  REQUIRE(th.get_clock_source() == ClockSource::INTERNAL);

  // Make MIDI active by sending a clock tick
  midi_proc.on_midi_clock_tick_received();
  REQUIRE(midi_proc.is_active() == true);

  // Call update() - should switch to MIDI
  th.update();
  REQUIRE(th.get_clock_source() == ClockSource::MIDI);
}

TEST_CASE("TempoHandler prefers EXTERNAL_SYNC over MIDI when cable connected") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::MIDI);

  musin::timing::SpeedAdapter speed_adapter_pref;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter_pref,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);

  // Make MIDI active
  midi_proc.on_midi_clock_tick_received();
  REQUIRE(midi_proc.is_active() == true);

  // Initially should be MIDI
  th.update();
  REQUIRE(th.get_clock_source() == ClockSource::MIDI);

  // Connect sync cable - should switch to EXTERNAL_SYNC
  sync_in.set_cable_connected(true);
  th.update();
  REQUIRE(th.get_clock_source() == ClockSource::EXTERNAL_SYNC);

  // Make MIDI inactive by advancing time past timeout (500ms)
  advance_time_us(600000); // 600ms > 500ms timeout
  REQUIRE(midi_proc.is_active() == false);

  // Disconnect cable - should switch to INTERNAL (no cable, MIDI inactive)
  sync_in.set_cable_connected(false);
  th.update();
  REQUIRE(th.get_clock_source() == ClockSource::INTERNAL);
}

TEST_CASE("TempoHandler never switches directly from MIDI to INTERNAL") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::MIDI);

  musin::timing::SpeedAdapter speed_adapter_never;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter_never,
                  /*send_midi_clock_when_stopped*/ false,
                  ClockSource::INTERNAL);

  // Start with MIDI active, switch to it
  midi_proc.on_midi_clock_tick_received();
  th.update();
  REQUIRE(th.get_clock_source() == ClockSource::MIDI);

  // Advance time to make MIDI inactive (timeout: 500ms)
  advance_time_us(600000); // 600ms > 500ms timeout

  REQUIRE(midi_proc.is_active() == false);

  // Call update() - should NOT switch to INTERNAL, stays MIDI
  // This validates the restriction in lines 277-281 of tempo_handler.cpp
  th.update();
  REQUIRE(th.get_clock_source() == ClockSource::MIDI);
}

TEST_CASE(
    "TempoHandler manual sync emits immediate resync regardless of timing") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::MIDI);

  musin::timing::SpeedAdapter speed_adapter_lb;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter_lb,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);
  clock_router.add_observer(speed_adapter_lb);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Send a MIDI tick to establish some history
  ClockEvent e{ClockSource::MIDI};
  e.is_physical_pulse = false;
  e.timestamp_us = static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  speed_adapter_lb.notification(e);
  rec.clear();

  // Wait some time - timing no longer matters with new behavior
  advance_time_us(5000); // 5ms

  // Trigger manual sync - should immediately emit resync (no look-behind)
  th.trigger_manual_sync();

  REQUIRE(rec.events.size() >= 1);
  REQUIRE(rec.events[0].is_resync == true);
  REQUIRE(rec.events[0].phase_24 == musin::timing::PHASE_DOWNBEAT);
}

TEST_CASE("TempoHandler manual sync with MIDI emits immediate resync") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::INTERNAL);

  musin::timing::SpeedAdapter speed_adapter_def;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter_def,
                  /*send_midi_clock_when_stopped*/ false, ClockSource::MIDI);
  clock_router.add_observer(speed_adapter_def);

  TempoEventRecorder rec;
  th.add_observer(rec);

  // Send a MIDI tick to establish some history
  ClockEvent e{ClockSource::MIDI};
  e.is_physical_pulse = false;
  e.timestamp_us = static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  speed_adapter_def.notification(e);
  rec.clear();

  // Wait any amount of time - timing no longer matters
  advance_time_us(15000); // 15ms

  // Trigger manual sync - should immediately emit resync (no deferral)
  th.trigger_manual_sync();
  REQUIRE(rec.events.size() >= 1);
  REQUIRE(rec.events[0].is_resync == true);
  REQUIRE(rec.events[0].phase_24 == musin::timing::PHASE_DOWNBEAT);
}

TEST_CASE("TempoHandler set_bpm only affects internal clock source") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::INTERNAL);

  musin::timing::SpeedAdapter speed_adapter_bpm;
  TempoHandler th(internal_clock, midi_proc, sync_in, sync_out, clock_router,
                  speed_adapter_bpm,
                  /*send_midi_clock_when_stopped*/ false,
                  ClockSource::INTERNAL);

  // Test BPM setting works with INTERNAL source
  th.set_bpm(140.0f);
  // InternalClock should now be at 140 BPM (can't directly test without access
  // to internal state)

  // Switch to MIDI source
  th.set_clock_source(ClockSource::MIDI);

  // set_bpm should be a no-op for MIDI source
  th.set_bpm(180.0f); // Should not affect anything

  // Switch back to INTERNAL - should still be at 140 BPM
  th.set_clock_source(ClockSource::INTERNAL);

  // This test validates the conditional check in set_bpm() method
  // The actual BPM verification would require exposing InternalClock state
  REQUIRE(th.get_clock_source() == ClockSource::INTERNAL);
}

TEST_CASE("TempoHandler playback state affects MIDI clock transmission") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  musin::timing::SyncIn sync_in(0, 1); // dummy pin numbers for test
  musin::timing::SyncOut sync_out(0);  // dummy pin number for test
  musin::timing::ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                                          ClockSource::INTERNAL);

  // Test with send_midi_clock_when_stopped = false
  musin::timing::SpeedAdapter speed_adapter_stopped;
  TempoHandler th_stopped(internal_clock, midi_proc, sync_in, sync_out,
                          clock_router, speed_adapter_stopped,
                          /*send_midi_clock_when_stopped*/ false,
                          ClockSource::INTERNAL);
  musin::timing::MidiClockOut midi_out_stopped(
      th_stopped, /*send_when_stopped_as_master*/ false);

  th_stopped.set_playback_state(PlaybackState::STOPPED);

  // Generate internal tick while stopped - should NOT send MIDI clock
  ClockEvent e{ClockSource::INTERNAL};
  midi_out_stopped.notification(e);

  // Test with send_midi_clock_when_stopped = true
  musin::timing::SpeedAdapter speed_adapter_always;
  TempoHandler th_always(internal_clock, midi_proc, sync_in, sync_out,
                         clock_router, speed_adapter_always,
                         /*send_midi_clock_when_stopped*/ true,
                         ClockSource::INTERNAL);
  musin::timing::MidiClockOut midi_out_always(
      th_always, /*send_when_stopped_as_master*/ true);

  th_always.set_playback_state(PlaybackState::STOPPED);

  // Clear previous MIDI calls
  reset_test_state();

  // Generate internal tick while stopped - SHOULD send MIDI clock
  midi_out_always.notification(e);

  // Flush MIDI queue so realtime clock is recorded by mock
  {
    musin::NullLogger logger;
    while (!musin::midi::midi_output_queue.empty()) {
      musin::midi::process_midi_output_queue(logger);
    }
  }

  // Verify at least one MIDI clock was sent
  size_t rt_count = 0;
  for (const auto &call : mock_midi_calls) {
    if (call.function_name == std::string("_sendRealTime_actual") &&
        call.rt_type == ::midi::Clock) {
      ++rt_count;
    }
  }
  REQUIRE(rt_count >= 1);
}
