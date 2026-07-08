#include "musin/timing/midi_clock_out.h"

#include "musin/midi/midi_wrapper.h"

namespace musin::timing {

MidiClockOut::MidiClockOut(musin::timing::TempoHandler &tempo_handler_ref,
                           bool send_when_stopped_din,
                           bool send_when_stopped_usb)
    : tempo_handler_(tempo_handler_ref),
      send_when_stopped_din_(send_when_stopped_din),
      send_when_stopped_usb_(send_when_stopped_usb) {
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
    // As clock sender, respect per-transport playback policy
    const bool playing =
        tempo_handler_.get_playback_state() == PlaybackState::PLAYING;
    if (playing || send_when_stopped_din_) {
      // Direct send bypasses queue to minimize jitter
      MIDI::internal::_sendRealTime_din_actual(midi::Clock);
    }
    if (playing || send_when_stopped_usb_) {
      MIDI::internal::_sendRealTime_usb_actual(midi::Clock);
    }
    return;
  }

  // Bridge EXTERNAL_SYNC to MIDI clock output
  if (event.source == ClockSource::EXTERNAL_SYNC) {
    // Direct send bypasses queue to minimize jitter
    MIDI::internal::_sendRealTime_actual(midi::Clock);
  }
}

} // namespace musin::timing
