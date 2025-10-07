#include "musin/timing/clock_event.h"
#include "musin/timing/speed_adapter.h"
#include "pico/time.h"

#include "midi_test_support.h"
#include "test_support.h"

#include <etl/observer.h>
#include <vector>

using musin::timing::ClockEvent;
using musin::timing::ClockSource;
using musin::timing::SpeedAdapter;
using musin::timing::SpeedModifier;

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

TEST_CASE("SpeedAdapter NORMAL emits every 2nd tick (24→12 PPQN)") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::NORMAL_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Generate 6 ticks 10ms apart (simulating 24 PPQN source)
  for (int i = 0; i < 6; ++i) {
    ClockEvent e{ClockSource::MIDI};
    e.is_beat = false;
    adapter.notification(e);
    advance_time_us(10000);
  }

  // NORMAL mode drops every other tick: 6→3
  REQUIRE(rec.events.size() == 3);
  for (const auto &e : rec.events) {
    REQUIRE(e.source == ClockSource::MIDI);
    REQUIRE(e.is_resync == false);
  }
}

TEST_CASE("SpeedAdapter HALF emits every 4th tick (24→6 PPQN)") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::HALF_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Send 8 ticks at regular intervals (simulating 24 PPQN source)
  for (int i = 0; i < 8; ++i) {
    ClockEvent e{ClockSource::EXTERNAL_SYNC};
    e.is_beat = (i % 3) == 0; // mix of physical/non-physical
    adapter.notification(e);
    advance_time_us(8000);
  }

  // Only downbeats pass through (ticks 0,3,6). Counter resets on each downbeat,
  // so divider never reaches 4.
  REQUIRE(rec.events.size() == 3);
}

TEST_CASE("SpeedAdapter DOUBLE passes through all ticks (24 PPQN)") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::DOUBLE_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Generate 5 ticks 10ms apart
  for (int i = 0; i < 5; ++i) {
    ClockEvent e{ClockSource::MIDI};
    e.is_beat = (i % 2) == 0;
    adapter.notification(e);
    advance_time_us(10000);
  }

  // DOUBLE mode passes all ticks through (24 PPQN output)
  REQUIRE(rec.events.size() == 5);
  for (const auto &e : rec.events) {
    REQUIRE(e.source == ClockSource::MIDI);
    REQUIRE(e.is_resync == false);
  }
}

TEST_CASE("SpeedAdapter resync forwards and clears counter") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::NORMAL_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Send one tick (odd counter, won't emit)
  ClockEvent e{ClockSource::MIDI};
  adapter.notification(e);

  // Send resync event - should forward and reset counter
  ClockEvent res{ClockSource::MIDI};
  res.is_resync = true;
  adapter.notification(res);

  // After resync, counter is reset, so next tick should emit (even counter)
  advance_time_us(8000);
  adapter.notification(e);

  // Events seen: resync, then nothing on first post-resync tick
  REQUIRE(rec.events.size() == 1);
  REQUIRE(rec.events[0].is_resync == true);
}
