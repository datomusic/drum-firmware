#include "midi_test_support.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "pico/time.h"
#include "test_support.h"

#include <etl/observer.h>
#include <vector>

using musin::timing::ClockEvent;
using musin::timing::ClockRouter;
using musin::timing::ClockSource;
using musin::timing::InternalClock;
using musin::timing::MidiClockProcessor;
using musin::timing::SyncIn;

namespace {
struct ClockEventRecorder : etl::observer<ClockEvent> {
  std::vector<ClockEvent> events;
  void notification(ClockEvent e) {
    events.push_back(e);
  }
  void clear() {
    events.clear();
  }
};

static void advance_time_us(uint64_t us) {
  advance_mock_time_us(us);
}
} // namespace

TEST_CASE("ClockRouter forwards ticks only from selected source") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  SyncIn sync_in(0, 1); // dummy pin numbers for test
  ClockRouter router(internal_clock, midi_proc, sync_in, ClockSource::INTERNAL);

  ClockEventRecorder rec;
  router.add_observer(rec);

  // Internal source should start; generate two internal ticks (~20833us apart)
  constexpr uint64_t tick_us = 20833;
  for (int i = 0; i < 2; ++i) {
    internal_clock.update(get_absolute_time());
    advance_time_us(tick_us + 10);
    internal_clock.update(get_absolute_time());
  }
  REQUIRE(rec.events.size() >= 2);
  for (const auto &e : rec.events) {
    REQUIRE(e.source == ClockSource::INTERNAL);
    REQUIRE(e.is_resync == false);
  }

  size_t count_after_internal = rec.events.size();

  // Switch to MIDI; should stop internal clock and enable MIDI echo
  router.set_clock_source(ClockSource::MIDI);
  REQUIRE(internal_clock.is_running() == false);
  REQUIRE(midi_proc.is_forward_echo_enabled() == true);

  size_t count_after_switch = rec.events.size();

  // Attempt to generate internal tick — should have no effect now
  internal_clock.update(get_absolute_time());
  REQUIRE(rec.events.size() == count_after_switch);

  // Now send a MIDI tick — first tick emits a resync then the raw tick
  midi_proc.on_midi_clock_tick_received();
  REQUIRE(rec.events.size() == count_after_switch + 2);
  REQUIRE(rec.events[count_after_switch].is_resync == true);
  REQUIRE(rec.events[count_after_switch].source == ClockSource::MIDI);
  REQUIRE(rec.events.back().source == ClockSource::MIDI);
  REQUIRE(rec.events.back().is_resync == false);
}

TEST_CASE("ClockRouter routes external sync directly and preserves "
          "physical flag") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  SyncIn sync_in(0, 1); // dummy pin numbers for test
  ClockRouter router(internal_clock, midi_proc, sync_in, ClockSource::INTERNAL);

  ClockEventRecorder rec;
  router.add_observer(rec);

  // Switch to EXTERNAL_SYNC; MIDI echo should be disabled
  router.set_clock_source(ClockSource::EXTERNAL_SYNC);
  REQUIRE(midi_proc.is_forward_echo_enabled() == false);

  size_t base_events = rec.events.size(); // includes resync from switch

  // Simulate an external physical pulse arriving directly via SyncIn
  ClockEvent pulse{ClockSource::EXTERNAL_SYNC};
  pulse.is_beat = true;
  router.notification(
      pulse); // Direct notification since SyncIn connects to router

  // Router should forward the event directly
  REQUIRE(rec.events.size() >= base_events + 1);

  // Last or one of the new events should be EXTERNAL_SYNC and preserve physical
  // flag
  bool found_physical = false;
  for (size_t i = base_events; i < rec.events.size(); ++i) {
    if (rec.events[i].source == ClockSource::EXTERNAL_SYNC &&
        rec.events[i].is_beat == true) {
      found_physical = true;
      break;
    }
  }
  REQUIRE(found_physical);
}

TEST_CASE("ClockRouter auto switching stays on MIDI once selected") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  SyncIn sync_in(0, 1);
  ClockRouter router(internal_clock, midi_proc, sync_in, ClockSource::INTERNAL);

  REQUIRE(router.get_clock_source() == ClockSource::INTERNAL);

  midi_proc.on_midi_clock_tick_received();
  REQUIRE(midi_proc.is_active());

  router.update_auto_source_switching();
  REQUIRE(router.get_clock_source() == ClockSource::MIDI);

  router.update_auto_source_switching();
  REQUIRE(router.get_clock_source() == ClockSource::MIDI);

  advance_time_us(21000);
  midi_proc.on_midi_clock_tick_received();
  REQUIRE(router.get_clock_source() == ClockSource::MIDI);

  advance_time_us(600000);
  router.update_auto_source_switching();
  REQUIRE(router.get_clock_source() == ClockSource::MIDI);
}
