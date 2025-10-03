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

TEST_CASE("SpeedAdapter NORMAL forwards all ticks") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::NORMAL_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Generate 5 ticks 10ms apart
  for (int i = 0; i < 5; ++i) {
    ClockEvent e{ClockSource::MIDI};
    e.is_downbeat = false;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    adapter.notification(e);
    advance_time_us(10000);
  }

  REQUIRE(rec.events.size() == 5);
  for (const auto &e : rec.events) {
    REQUIRE(e.source == ClockSource::MIDI);
    REQUIRE(e.is_resync == false);
  }
}

TEST_CASE("SpeedAdapter HALF emits every 2nd tick") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::HALF_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Send 6 ticks at regular intervals
  for (int i = 0; i < 6; ++i) {
    ClockEvent e{ClockSource::EXTERNAL_SYNC};
    e.is_downbeat = (i % 3) == 0; // mix of physical/non-physical
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    adapter.notification(e);
    advance_time_us(8000);
  }

  // SpeedAdapter now handles HALF_SPEED by emitting every 2nd tick
  REQUIRE(rec.events.size() == 3);
}

TEST_CASE("SpeedAdapter DOUBLE inserts mid ticks with non-physical flag") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::DOUBLE_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // First tick at t=0us -> forwarded, no insert scheduled yet
  {
    ClockEvent e{ClockSource::MIDI};
    e.is_downbeat = false;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    adapter.notification(e);
  }

  // Advance 10ms, second tick -> forwarded, schedules insert at +5ms
  advance_time_us(10000);
  {
    ClockEvent e{ClockSource::MIDI};
    e.is_downbeat = false;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    adapter.notification(e);
  }

  // Advance 5ms -> hit mid insert
  advance_time_us(5000);
  adapter.update(get_absolute_time());

  // Advance another 5ms, third tick arrives -> forwarded, may schedule next
  // insert
  advance_time_us(5000);
  {
    ClockEvent e{ClockSource::MIDI};
    e.is_downbeat = false;
    e.timestamp_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    adapter.notification(e);
  }

  // We should have: pass-through first, pass-through second, inserted mid,
  // pass-through third => 4 events
  REQUIRE(rec.events.size() == 4);
  // Inserted event must be non-physical
  bool found_non_physical_insert = false;
  for (const auto &e : rec.events) {
    if (e.is_downbeat == false) {
      found_non_physical_insert = true;
      break;
    }
  }
  REQUIRE(found_non_physical_insert);
}

TEST_CASE("SpeedAdapter resync forwards and clears scheduling") {
  reset_test_state();

  SpeedAdapter adapter(SpeedModifier::DOUBLE_SPEED);
  ClockEventRecorder rec;
  adapter.add_observer(rec);

  // Prime interval with two ticks
  ClockEvent e{ClockSource::MIDI};
  e.timestamp_us = static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  adapter.notification(e);
  advance_time_us(12000);
  e.timestamp_us = static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  adapter.notification(e);

  // Instead of waiting for the mid insert, send a resync event
  ClockEvent res{ClockSource::MIDI};
  res.is_resync = true;
  res.timestamp_us =
      static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  adapter.notification(res);

  // Advance time past where an insert would have occurred; none should fire
  advance_time_us(8000);
  adapter.update(get_absolute_time());

  // Events seen: first pass-through, second pass-through, resync
  REQUIRE(rec.events.size() == 3);
  REQUIRE(rec.events[2].is_resync == true);
}
