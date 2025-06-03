#include "musin/timing/tempo_handler.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_event.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::sendRealTime
#include "midi_Defs.h"               // For midi::Clock

// Include headers for specific clock types if needed for identification
// #include "internal_clock.h"
// #include "midi_clock.h"
// #include "external_sync_clock.h"

namespace musin::timing {

TempoHandler::TempoHandler(InternalClock &internal_clock_ref, ClockSource initial_source)
    : _internal_clock_ref(internal_clock_ref), current_source_(initial_source),
      _playback_state(PlaybackState::STOPPED) {
  // If managing clock instances internally or needing references:
  // Initialize clock references/pointers here, potentially passed via constructor.
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source != current_source_) {
    current_source_ = source;
    // Add logic here if switching sources requires actions like:
    // - Detaching from the old source's observable notifications
    // - Attaching to the new source's observable notifications
    // - Resetting tempo calculation state
  }
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  // Only process and forward ticks if they come from the currently selected source
  if (event.source == current_source_) {
    // If the clock source is internal and playback is active, send MIDI clock
    if (current_source_ == ClockSource::INTERNAL && _playback_state == PlaybackState::PLAYING) {
      MIDI::sendRealTime(midi::Clock);
    }

    musin::timing::TempoEvent tempo_tick_event;
    // Populate TempoEvent with timestamp or other data if needed later
    etl::observable<etl::observer<musin::timing::TempoEvent>,
                    MAX_TEMPO_OBSERVERS>::notify_observers(tempo_tick_event);
  }
}

void TempoHandler::set_bpm(float bpm) {
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.set_bpm(bpm);
  }
  // If the source is not internal, this call has no effect on the active clock's BPM.
  // The internal clock's BPM will be updated, but it won't be used until
  // ClockSource::INTERNAL is selected again.
}

void TempoHandler::set_playback_state(PlaybackState new_state) {
  _playback_state = new_state;
}

} // namespace musin::timing
