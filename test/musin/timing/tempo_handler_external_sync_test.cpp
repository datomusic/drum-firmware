
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

TEST_CASE("TempoHandler external manual sync primes next SyncIn downbeat") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in_stub(0, 1);
  musin::timing::ClockRouter clock_router(
      internal_clock, midi_processor, sync_in_stub, ClockSource::EXTERNAL_SYNC);

  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);

  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  // Simulate pressing PLAY: manual sync intent should prime the next SyncIn
  // downbeat to pass through (clearing waiting_for_external_downbeat_).
  tempo_handler.trigger_manual_sync();
  REQUIRE(recorder.events.empty());

  ClockEvent physical_pulse{ClockSource::EXTERNAL_SYNC};
  physical_pulse.is_beat = true;
  clock_router.notification(physical_pulse);

  REQUIRE_FALSE(recorder.events.empty());
  // Manual sync primes the downbeat but doesn't mark as resync for external
  // sync
  REQUIRE(recorder.events.front().is_resync == false);
  REQUIRE(recorder.events.front().phase_12 == musin::timing::PHASE_DOWNBEAT);
}

TEST_CASE("TempoHandler speed change with pending downbeat waits then aligns") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in_stub(0, 1);
  musin::timing::ClockRouter clock_router(
      internal_clock, midi_processor, sync_in_stub, ClockSource::EXTERNAL_SYNC);

  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);

  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  tempo_handler.trigger_manual_sync();
  REQUIRE(recorder.events.empty());

  // Change speed to DOUBLE while waiting for downbeat
  tempo_handler.set_speed_modifier(SpeedModifier::DOUBLE_SPEED);
  REQUIRE(recorder.events.empty());

  // Regular ticks (not downbeats) should be suppressed
  ClockEvent regular_tick{ClockSource::EXTERNAL_SYNC};
  regular_tick.is_beat = false;
  clock_router.notification(regular_tick);
  REQUIRE(recorder.events.empty());

  // Downbeat arrives - should align to phase 0 for DOUBLE_SPEED
  ClockEvent downbeat{ClockSource::EXTERNAL_SYNC};
  downbeat.is_beat = true;
  clock_router.notification(downbeat);

  REQUIRE(recorder.events.size() == 1);
  REQUIRE(recorder.events.front().phase_12 == 0);
  REQUIRE(recorder.events.front().is_resync == false);
}

TEST_CASE("TempoHandler cable disconnect during pending sync switches source") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in_stub(0, 1);
  musin::timing::ClockRouter clock_router(
      internal_clock, midi_processor, sync_in_stub, ClockSource::EXTERNAL_SYNC);

  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);

  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  sync_in_stub.set_cable_connected(true);
  tempo_handler.trigger_manual_sync();
  REQUIRE(recorder.events.empty());

  // Cable disconnect triggers auto source switching
  sync_in_stub.set_cable_connected(false);
  clock_router.update_auto_source_switching();

  // Should switch to INTERNAL and reset phase
  REQUIRE(tempo_handler.get_clock_source() == ClockSource::INTERNAL);

  // Internal clock tick should now produce events (not suppressed)
  ClockEvent internal_tick{ClockSource::INTERNAL};
  clock_router.notification(internal_tick);

  REQUIRE_FALSE(recorder.events.empty());
}

TEST_CASE(
    "TempoHandler manual source switch with pending downbeat clears wait") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in_stub(0, 1);
  musin::timing::ClockRouter clock_router(
      internal_clock, midi_processor, sync_in_stub, ClockSource::EXTERNAL_SYNC);

  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);

  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  tempo_handler.trigger_manual_sync();
  REQUIRE(recorder.events.empty());

  // Switch to INTERNAL before external downbeat arrives
  tempo_handler.set_clock_source(ClockSource::INTERNAL);

  // Internal ticks should now work immediately
  ClockEvent internal_tick{ClockSource::INTERNAL};
  clock_router.notification(internal_tick);

  REQUIRE_FALSE(recorder.events.empty());
}

TEST_CASE("TempoHandler multiple speed changes before downbeat uses final "
          "modifier") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in_stub(0, 1);
  musin::timing::ClockRouter clock_router(
      internal_clock, midi_processor, sync_in_stub, ClockSource::EXTERNAL_SYNC);

  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);

  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  tempo_handler.trigger_manual_sync();

  // Rapid speed changes: HALF → DOUBLE → NORMAL
  tempo_handler.set_speed_modifier(SpeedModifier::HALF_SPEED);
  tempo_handler.set_speed_modifier(SpeedModifier::DOUBLE_SPEED);
  tempo_handler.set_speed_modifier(SpeedModifier::NORMAL_SPEED);

  REQUIRE(recorder.events.empty());

  // Advance phase to non-zero for NORMAL alignment test
  ClockEvent non_downbeat{ClockSource::EXTERNAL_SYNC};
  non_downbeat.is_beat = false;
  for (int i = 0; i < 5; ++i) {
    clock_router.notification(non_downbeat);
  }

  // Downbeat with NORMAL speed should align to quarter-note grid (0,3,6,9)
  ClockEvent downbeat{ClockSource::EXTERNAL_SYNC};
  downbeat.is_beat = true;
  clock_router.notification(downbeat);

  REQUIRE(recorder.events.size() == 1);
  // Phase 5 should align to 6 for NORMAL speed
  uint8_t aligned_phase = recorder.events.front().phase_12;
  REQUIRE((aligned_phase == 0 || aligned_phase == 3 || aligned_phase == 6 ||
           aligned_phase == 9));
}

TEST_CASE("TempoHandler speed change without pending downbeat realigns on next "
          "beat") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in_stub(0, 1);
  musin::timing::ClockRouter clock_router(
      internal_clock, midi_processor, sync_in_stub, ClockSource::EXTERNAL_SYNC);

  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);

  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  // Establish initial sync with first downbeat
  tempo_handler.trigger_manual_sync();
  ClockEvent first_downbeat{ClockSource::EXTERNAL_SYNC};
  first_downbeat.is_beat = true;
  clock_router.notification(first_downbeat);

  REQUIRE(recorder.events.size() == 1);
  REQUIRE(recorder.events.back().phase_12 == 0);
  recorder.clear();

  // Advance several ticks
  ClockEvent regular_tick{ClockSource::EXTERNAL_SYNC};
  regular_tick.is_beat = false;
  for (int i = 0; i < 7; ++i) {
    clock_router.notification(regular_tick);
  }

  REQUIRE(recorder.events.size() == 3);
  REQUIRE(recorder.events.back().phase_12 == 3);
  recorder.clear();

  // Change speed to DOUBLE (should align to downbeat on next beat)
  tempo_handler.set_speed_modifier(SpeedModifier::DOUBLE_SPEED);

  // Next downbeat should re-align to phase 0
  ClockEvent second_downbeat{ClockSource::EXTERNAL_SYNC};
  second_downbeat.is_beat = true;
  clock_router.notification(second_downbeat);

  REQUIRE(recorder.events.size() == 1);
  REQUIRE(recorder.events.front().phase_12 == 0);
  REQUIRE(recorder.events.front().is_resync == false);
}
