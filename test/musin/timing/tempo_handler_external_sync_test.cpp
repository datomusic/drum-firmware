
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
