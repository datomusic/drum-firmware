#include "musin/timing/tempo_handler.h"
#include "midi_Defs.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/timing_constants.h"

namespace musin::timing {

TempoHandler::TempoHandler(InternalClock &internal_clock_ref,
                           MidiClockProcessor &midi_clock_processor_ref,
                           SyncIn &sync_in_ref,
                           bool send_midi_clock_when_stopped,
                           ClockSource initial_source)
    : _internal_clock_ref(internal_clock_ref),
      _midi_clock_processor_ref(midi_clock_processor_ref),
      _sync_in_ref(sync_in_ref), current_source_(initial_source),
      _playback_state(PlaybackState::STOPPED),
      _send_this_internal_tick_as_midi_clock(true),
      _send_midi_clock_when_stopped(send_midi_clock_when_stopped) {

  _internal_clock_ref.add_observer(*this);
  _midi_clock_processor_ref.add_observer(*this);
  _sync_in_ref.add_observer(*this);

  set_clock_source(initial_source);
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source == current_source_) {
    return;
  }

  current_source_ = source;

  uint32_t ppqn = musin::timing::DEFAULT_PPQN;
  if (source == ClockSource::EXTERNAL_SYNC) {
    ppqn = _sync_in_ref.get_ppqn();
  }

  _internal_clock_ref.set_discipline(source, ppqn);
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  switch (event.source) {
  case ClockSource::INTERNAL: {
    // This is a disciplined tick from our own PLL. Forward it.
    musin::timing::TempoEvent tempo_tick_event{};
    etl::observable<etl::observer<musin::timing::TempoEvent>,
                    MAX_TEMPO_OBSERVERS>::notify_observers(tempo_tick_event);

    // Also, handle MIDI clock output if we are the master.
    if (_playback_state == PlaybackState::PLAYING ||
        _send_midi_clock_when_stopped) {
      if (_send_this_internal_tick_as_midi_clock) {
        MIDI::sendRealTime(midi::Clock);
      }
      _send_this_internal_tick_as_midi_clock =
          !_send_this_internal_tick_as_midi_clock;
    } else {
      _send_this_internal_tick_as_midi_clock = true;
    }
    break;
  }

  case ClockSource::MIDI:
  case ClockSource::EXTERNAL_SYNC: {
    // These are reference ticks. Feed them to the PLL. Do not forward.
    _internal_clock_ref.reference_tick_received(get_absolute_time(),
                                                event.source);
    break;
  }
  }
}

void TempoHandler::set_bpm(float bpm) {
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.set_bpm(bpm);
  }
}

void TempoHandler::set_playback_state(PlaybackState new_state) {
  _playback_state = new_state;
  if (current_source_ == ClockSource::INTERNAL &&
      _playback_state == PlaybackState::STOPPED) {
    _send_this_internal_tick_as_midi_clock = true;
  }
}

void TempoHandler::update() {
  if (_sync_in_ref.is_cable_connected()) {
    if (current_source_ != ClockSource::EXTERNAL_SYNC) {
      set_clock_source(ClockSource::EXTERNAL_SYNC);
    }
  } else {
    if (_midi_clock_processor_ref.get_derived_bpm() > 0.0f) {
      if (current_source_ != ClockSource::MIDI) {
        set_clock_source(ClockSource::MIDI);
      }
    } else {
      if (current_source_ != ClockSource::INTERNAL) {
        set_clock_source(ClockSource::INTERNAL);
      }
    }
  }
}

} // namespace musin::timing