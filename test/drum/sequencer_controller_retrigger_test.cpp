#include "drum/config.h"
#include "drum/events.h"
#include "drum/sequencer_controller.h"
#include "musin/hal/null_logger.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/speed_adapter.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"
#include "pico/time.h"

#include <catch2/catch_test_macros.hpp>
#include <etl/observer.h>
#include <vector>

using drum::RetriggerMode;
using drum::SequencerController;
using musin::timing::ClockRouter;
using musin::timing::ClockSource;
using musin::timing::InternalClock;
using musin::timing::MidiClockProcessor;
using musin::timing::SpeedAdapter;
using musin::timing::SyncIn;
using musin::timing::TempoEvent;
using musin::timing::TempoHandler;

namespace {

// Captures every NoteEvent notified by the SequencerController under test.
struct NoteEventRecorder : etl::observer<drum::Events::NoteEvent> {
  std::vector<drum::Events::NoteEvent> events;
  void notification(drum::Events::NoteEvent event) {
    events.push_back(event);
  }
};

} // namespace

// Regression test for the bug where a step-boundary tick in Step retrigger
// mode fires two identical NoteEvents instead of one: notification()
// marks the track's retrigger bit due via mark_due_tracks(), then update()
// fires trigger_note_on() once for the due-mask bit and a second time in the
// _step_is_due block because the track's mode is also Step. See
// SequencerController::update() in drum/sequencer_controller.cpp.
TEST_CASE("Step retrigger fires exactly one NoteEvent per step boundary") {
  // Minimal real TempoHandler wiring; the tempo source is never driven
  // through its usual clock chain -- notification() is invoked directly to
  // exercise the look-behind step scheduling logic in isolation.
  InternalClock internal_clock(120.0f);
  MidiClockProcessor midi_proc;
  SyncIn sync_in(0, 1);
  ClockRouter clock_router(internal_clock, midi_proc, sync_in,
                           ClockSource::INTERNAL);
  SpeedAdapter speed_adapter;
  TempoHandler tempo_handler(clock_router, speed_adapter,
                             /*send_midi_clock_when_stopped*/ false,
                             ClockSource::INTERNAL);

  musin::NullLogger logger;
  SequencerController<drum::config::NUM_TRACKS,
                      drum::config::NUM_STEPS_PER_TRACK>
      controller(tempo_handler, logger);

  NoteEventRecorder recorder;
  controller.add_observer(recorder);

  constexpr uint8_t TRACK_UNDER_TEST = 0;
  controller.activate_play_on_every_step(TRACK_UNDER_TEST, RetriggerMode::Step);

  // Leave the sequencer's own step disabled so the only NoteEvents that can
  // appear are the ones the retrigger effect produces at the step boundary.
  auto &track = controller.get_sequencer().get_track(TRACK_UNDER_TEST);
  track.get_step(0).enabled = false;

  controller.start();

  // Prime last_phase_12_ to 11 without crossing the step-0 anchor (expected
  // phase 0 with swing disabled), then advance to phase 0 to make the
  // downbeat step boundary due via the wrap-around look-behind window.
  controller.notification(TempoEvent{.phase_12 = 11, .is_resync = false});
  controller.notification(TempoEvent{.phase_12 = 0, .is_resync = false});

  controller.update();

  std::size_t retrigger_notes_on_track = 0;
  for (const auto &event : recorder.events) {
    if (event.track_index == TRACK_UNDER_TEST &&
        event.velocity == drum::config::drumpad::RETRIGGER_VELOCITY) {
      ++retrigger_notes_on_track;
    }
  }

  REQUIRE(retrigger_notes_on_track == 1);
}
