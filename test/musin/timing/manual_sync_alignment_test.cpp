#include "midi_test_support.h"
#include "pico/time.h"
#include "test_support.h"

#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/speed_adapter.h"
#include "musin/timing/sync_in.h" // Test override provides stub
#include "musin/timing/tempo_handler.h"
#include "musin/timing/timing_constants.h"

#include <etl/observer.h>
#include <optional>
#include <vector>

// The play button resync has to land the sequencer's steps on the same raw
// 24-PPQN ticks as the SyncOut pulse, no matter how many raw ticks have
// already elapsed when the button is pressed. SyncOut itself needs pico
// alarms and is stubbed out in the host build, so these tests cover the
// SpeedAdapter/TempoHandler side: the resync must restart the 24->12 divider
// so that the sixth tempo tick always falls on the twelfth raw tick after the
// press, which is exactly where SyncOut's 12-tick countdown pulses.

using musin::timing::ClockEvent;
using musin::timing::ClockSource;
using musin::timing::InternalClock;
using musin::timing::MidiClockProcessor;
using musin::timing::SpeedModifier;
using musin::timing::TempoEvent;
using musin::timing::TempoHandler;

namespace {

struct TempoEventRecorder : etl::observer<TempoEvent> {
  std::vector<TempoEvent> events;
  void notification(TempoEvent e) override {
    events.push_back(e);
  }
  void clear() {
    events.clear();
  }
};

struct ClockEventRecorder : etl::observer<ClockEvent> {
  std::vector<ClockEvent> events;
  void notification(ClockEvent e) override {
    events.push_back(e);
  }
  void clear() {
    events.clear();
  }
};

// Real InternalClock/ClockRouter/SpeedAdapter/TempoHandler chain, wired the
// same way drum/main.cpp wires it (SyncOut omitted: host stub).
struct TimingChain {
  InternalClock internal_clock{120.0f};
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in{0, 1};
  musin::timing::ClockRouter clock_router{internal_clock, midi_processor,
                                          sync_in, ClockSource::INTERNAL};
  musin::timing::SpeedAdapter speed_adapter{SpeedModifier::NORMAL_SPEED};
  TempoHandler tempo_handler{clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::INTERNAL};
  TempoEventRecorder tempo_recorder;

  TimingChain() {
    clock_router.add_observer(speed_adapter);
    tempo_handler.add_observer(tempo_recorder);
  }

  void raw_tick() {
    ClockEvent tick{ClockSource::INTERNAL};
    clock_router.notification(tick);
  }
};

// Index of the raw tick (1-based, counted from the resync) on which the tempo
// phase first reaches `phase`, or nullopt if it never does.
std::optional<int> raw_tick_of_phase(TimingChain &chain, uint8_t phase,
                                     int raw_ticks) {
  for (int raw = 1; raw <= raw_ticks; ++raw) {
    chain.tempo_recorder.clear();
    chain.raw_tick();
    for (const auto &event : chain.tempo_recorder.events) {
      if (event.phase_12 == phase) {
        return raw;
      }
    }
  }
  return std::nullopt;
}

constexpr int RAW_TICKS_PER_SYNC_PULSE = 12;

} // namespace

TEST_CASE("Manual sync aligns steps to the sync pulse after an odd raw tick "
          "count") {
  reset_test_state();
  TimingChain chain;

  // Odd number of raw ticks before the press leaves the 24->12 divider on an
  // odd count; without a divider reset the next tempo tick would arrive one
  // raw tick early.
  for (int i = 0; i < 7; ++i) {
    chain.raw_tick();
  }

  constexpr uint8_t ANCHOR = 0;
  chain.tempo_recorder.clear();
  chain.tempo_handler.trigger_manual_sync(ANCHOR);

  auto tick = raw_tick_of_phase(chain, ANCHOR + 6, RAW_TICKS_PER_SYNC_PULSE);
  REQUIRE(tick.has_value());
  REQUIRE(*tick == RAW_TICKS_PER_SYNC_PULSE);
}

TEST_CASE("Manual sync aligns steps to the sync pulse after an even raw tick "
          "count") {
  reset_test_state();
  TimingChain chain;

  for (int i = 0; i < 8; ++i) {
    chain.raw_tick();
  }

  constexpr uint8_t ANCHOR = 0;
  chain.tempo_recorder.clear();
  chain.tempo_handler.trigger_manual_sync(ANCHOR);

  auto tick = raw_tick_of_phase(chain, ANCHOR + 6, RAW_TICKS_PER_SYNC_PULSE);
  REQUIRE(tick.has_value());
  REQUIRE(*tick == RAW_TICKS_PER_SYNC_PULSE);
}

TEST_CASE("Manual sync alignment is independent of the raw tick parity") {
  reset_test_state();

  std::vector<int> landing_ticks;
  for (int preroll = 0; preroll < 8; ++preroll) {
    TimingChain chain;
    for (int i = 0; i < preroll; ++i) {
      chain.raw_tick();
    }
    chain.tempo_recorder.clear();
    chain.tempo_handler.trigger_manual_sync(musin::timing::PHASE_DOWNBEAT);

    auto tick = raw_tick_of_phase(chain, musin::timing::PHASE_DOWNBEAT + 6,
                                  RAW_TICKS_PER_SYNC_PULSE);
    REQUIRE(tick.has_value());
    landing_ticks.push_back(*tick);
  }

  for (int landing : landing_ticks) {
    REQUIRE(landing == RAW_TICKS_PER_SYNC_PULSE);
  }
}

TEST_CASE("Manual sync with an offbeat anchor still lands on the sync pulse") {
  reset_test_state();
  TimingChain chain;

  for (int i = 0; i < 5; ++i) {
    chain.raw_tick();
  }

  constexpr uint8_t ANCHOR = 6;
  chain.tempo_recorder.clear();
  chain.tempo_handler.trigger_manual_sync(ANCHOR);

  REQUIRE(chain.tempo_recorder.events.size() == 1);
  REQUIRE(chain.tempo_recorder.events[0].is_resync == true);
  REQUIRE(chain.tempo_recorder.events[0].phase_12 == ANCHOR);

  constexpr uint8_t EXPECTED =
      (ANCHOR + 6) % musin::timing::DEFAULT_PPQN; // wraps to 0
  auto tick = raw_tick_of_phase(chain, EXPECTED, RAW_TICKS_PER_SYNC_PULSE);
  REQUIRE(tick.has_value());
  REQUIRE(*tick == RAW_TICKS_PER_SYNC_PULSE);
}

TEST_CASE("Manual sync emits exactly one resync TempoEvent carrying the "
          "anchor") {
  reset_test_state();
  TimingChain chain;

  for (int i = 0; i < 3; ++i) {
    chain.raw_tick();
  }

  constexpr uint8_t ANCHOR = 6;
  chain.tempo_recorder.clear();
  chain.tempo_handler.trigger_manual_sync(ANCHOR);

  REQUIRE(chain.tempo_recorder.events.size() == 1);
  REQUIRE(chain.tempo_recorder.events[0].is_resync == true);
  REQUIRE(chain.tempo_recorder.events[0].phase_12 == ANCHOR);

  // The following raw ticks must not repeat the resync.
  for (int i = 0; i < RAW_TICKS_PER_SYNC_PULSE; ++i) {
    chain.raw_tick();
  }
  size_t resync_count = 0;
  for (const auto &event : chain.tempo_recorder.events) {
    if (event.is_resync) {
      ++resync_count;
    }
  }
  REQUIRE(resync_count == 1);
}

TEST_CASE("Manual sync travels through the clock router") {
  reset_test_state();
  TimingChain chain;

  // A router observer stands in for SyncOut, which pulses on the resync event
  // and restarts its 12 raw tick countdown from there.
  ClockEventRecorder router_recorder;
  chain.clock_router.add_observer(router_recorder);

  chain.raw_tick();
  router_recorder.clear();

  chain.tempo_handler.trigger_manual_sync(musin::timing::PHASE_DOWNBEAT);

  size_t resync_count = 0;
  for (const auto &event : router_recorder.events) {
    if (event.is_resync) {
      ++resync_count;
    }
  }
  REQUIRE(resync_count == 1);
}

TEST_CASE("Manual sync on external sync still waits for the downbeat") {
  reset_test_state();

  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_processor;
  musin::timing::SyncIn sync_in(0, 1);
  musin::timing::ClockRouter clock_router(internal_clock, midi_processor,
                                          sync_in, ClockSource::EXTERNAL_SYNC);
  musin::timing::SpeedAdapter speed_adapter(SpeedModifier::NORMAL_SPEED);
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::EXTERNAL_SYNC);
  clock_router.add_observer(speed_adapter);

  TempoEventRecorder recorder;
  tempo_handler.add_observer(recorder);

  tempo_handler.trigger_manual_sync(musin::timing::PHASE_DOWNBEAT);
  REQUIRE(recorder.events.empty());

  ClockEvent downbeat{ClockSource::EXTERNAL_SYNC};
  downbeat.is_beat = true;
  clock_router.notification(downbeat);

  REQUIRE(recorder.events.size() == 1);
  REQUIRE(recorder.events[0].is_resync == false);
  REQUIRE(recorder.events[0].phase_12 == musin::timing::PHASE_DOWNBEAT);
}
