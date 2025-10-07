
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
