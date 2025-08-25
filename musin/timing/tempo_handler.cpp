#include "musin/timing/tempo_handler.h"
#include "midi_Defs.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_event.h"

namespace musin::timing {

TempoHandler::TempoHandler(InternalClock &internal_clock_ref,
                           MidiClockProcessor &midi_clock_processor_ref,
                           SyncIn &sync_in_ref,
                           ClockMultiplier &clock_multiplier_ref,
                           bool send_midi_clock_when_stopped,
                           ClockSource initial_source)
    : _internal_clock_ref(internal_clock_ref),
      _midi_clock_processor_ref(midi_clock_processor_ref),
      _sync_in_ref(sync_in_ref), _clock_multiplier_ref(clock_multiplier_ref),
      current_source_(initial_source), _playback_state(PlaybackState::STOPPED),
      current_speed_modifier_(SpeedModifier::NORMAL_SPEED),
      midi_tick_counter_(0), _send_this_internal_tick_as_midi_clock(true),
      _send_midi_clock_when_stopped(send_midi_clock_when_stopped) {

  set_clock_source(initial_source);
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source == current_source_) {
    return;
  }

  // Cleanup for the old source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.remove_observer(*this);
    _internal_clock_ref.stop();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.remove_observer(*this);
    _midi_clock_processor_ref.reset();
  } else if (current_source_ == ClockSource::EXTERNAL_SYNC) {
    _clock_multiplier_ref.remove_observer(*this);
    _clock_multiplier_ref.reset();
  }

  current_source_ = source;

  // Setup for the new source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.add_observer(*this);
    _send_this_internal_tick_as_midi_clock = true;
    _internal_clock_ref.start();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.add_observer(*this);
  } else if (current_source_ == ClockSource::EXTERNAL_SYNC) {
    _clock_multiplier_ref.add_observer(*this);
  }
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  if (event.source == ClockSource::INTERNAL &&
      current_source_ == ClockSource::INTERNAL) {
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
  }

  if (event.source != current_source_) {
    return;
  }

  if (current_source_ == ClockSource::MIDI) {
    switch (current_speed_modifier_) {
    case SpeedModifier::NORMAL_SPEED: {
      musin::timing::TempoEvent tempo_tick_event{};
      notify_observers(tempo_tick_event);
      break;
    }
    case SpeedModifier::HALF_SPEED: {
      midi_tick_counter_++;
      if (midi_tick_counter_ >= 2) {
        musin::timing::TempoEvent tempo_tick_event{};
        notify_observers(tempo_tick_event);
        midi_tick_counter_ = 0;
      }
      break;
    }
    case SpeedModifier::DOUBLE_SPEED: {
      musin::timing::TempoEvent tempo_tick_event{};
      notify_observers(tempo_tick_event);
      notify_observers(tempo_tick_event);
      break;
    }
    }
  } else {
    musin::timing::TempoEvent tempo_tick_event{};
    notify_observers(tempo_tick_event);
  }
}

void TempoHandler::set_bpm(float bpm) {
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.set_bpm(bpm);
  }
}

void TempoHandler::set_speed_modifier(SpeedModifier modifier) {
  current_speed_modifier_ = modifier;
  _clock_multiplier_ref.set_speed_modifier(modifier);
  midi_tick_counter_ = 0;
}

void TempoHandler::set_playback_state(PlaybackState new_state) {
  _playback_state = new_state;
  if (current_source_ == ClockSource::INTERNAL &&
      _playback_state == PlaybackState::STOPPED) {
    _send_this_internal_tick_as_midi_clock = true;
  }
}

/**
 * @brief Automatically switches the clock source based on a fixed priority.
 *
 * This method implements the automatic clock source selection logic, which
 * is called periodically from the main loop. The priority is as follows:
 * 1. External Sync: If a sync cable is connected, it will always be the
 *    selected source.
 * 2. MIDI Clock: If no sync cable is connected, it checks for an active MIDI
 *    clock signal. If MIDI clock is being received, it becomes the source.
 * 3. Internal Clock: If neither External Sync nor MIDI clock is available,
 *    the system falls back to the internal clock.
 */
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