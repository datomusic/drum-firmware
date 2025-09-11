#include "musin/timing/midi_clock_out.h"

#include "musin/midi/midi_wrapper.h"

namespace musin::timing {

MidiClockOut::MidiClockOut(musin::timing::TempoHandler &tempo_handler_ref,
                           bool send_when_stopped_as_master)
    : tempo_handler_(tempo_handler_ref),
      send_when_stopped_(send_when_stopped_as_master) {
}

void MidiClockOut::notification(musin::timing::ClockEvent event) {
  if (event.is_resync) {
    return;
  }

  // For MIDI source, immediate echo is handled by MidiClockProcessor
  if (event.source == ClockSource::MIDI) {
    return;
  }

  if (event.source == ClockSource::INTERNAL) {
    // As clock sender, respect playback policy
    if (tempo_handler_.get_playback_state() == PlaybackState::PLAYING ||
        send_when_stopped_) {
      MIDI::sendRealTime(midi::Clock);
    }
    return;
  }

  // Bridge EXTERNAL_SYNC to MIDI clock output
  if (event.source == ClockSource::EXTERNAL_SYNC) {
    MIDI::sendRealTime(midi::Clock);
  }
}

} // namespace musin::timing

