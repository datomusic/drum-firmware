#include "musin/timing/tempo_handler.h"
#include "drum/config.h"             // For drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER
#include "midi_Defs.h"               // For midi::Clock
#include "musin/midi/midi_wrapper.h" // For MIDI::sendRealTime
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_event.h"

// Include headers for specific clock types if needed for identification
// #include "internal_clock.h"
#include "musin/timing/midi_clock_processor.h" // Added include
// #include "external_sync_clock.h"

namespace musin::timing {

TempoHandler::TempoHandler(InternalClock &internal_clock_ref,
                           MidiClockProcessor &midi_clock_processor_ref, ClockSource initial_source)
    : _internal_clock_ref(internal_clock_ref), _midi_clock_processor_ref(midi_clock_processor_ref),
      current_source_(initial_source), // Initialize current_source_ directly with initial_source
      _playback_state(PlaybackState::STOPPED),
      _send_this_internal_tick_as_midi_clock(true) { // Initialize flag

  // Directly set up the initial source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.add_observer(*this);
    _internal_clock_ref.start();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.add_observer(*this);
    // MidiClockProcessor is driven by external MIDI ticks, no explicit start from here.
  }
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source == current_source_) {
    return; // No change needed
  }

  // Cleanup for the old source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.remove_observer(*this);
    _internal_clock_ref.stop();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.remove_observer(*this);
    _midi_clock_processor_ref.reset(); // Reset when switching away from MIDI
  }

  current_source_ = source;

  // Setup for the new source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.add_observer(*this);
    _send_this_internal_tick_as_midi_clock = true; // Reset for consistent start
    _internal_clock_ref.start();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.add_observer(*this); // Observe when MIDI is the source
    // MidiClockProcessor is driven by external MIDI ticks.
    // Internal clock was stopped above if it was the previous source.
  }
  // printf("TempoHandler: Switched clock source to %s\n", current_source_ == ClockSource::INTERNAL
  // ? "INTERNAL" : "MIDI");
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  // If a MIDI clock event arrives and we are not yet on MIDI source,
  // it's a strong indication to switch if MidiClockProcessor is now active.
  if (event.source == ClockSource::MIDI && current_source_ != ClockSource::MIDI) {
    if (_midi_clock_processor_ref.get_derived_bpm() > 0.0f) {
      // printf("TempoHandler: MIDI clock event received while not on MIDI source. Switching to
      // MIDI.\n");
      set_clock_source(ClockSource::MIDI);
    }
  }

  // Handle MIDI Clock output if internal source is master
  // This should only happen if the event is from InternalClock AND current_source_ is INTERNAL
  if (event.source == ClockSource::INTERNAL && current_source_ == ClockSource::INTERNAL) {
    if (_playback_state == PlaybackState::PLAYING ||
        drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER) {
      if (_send_this_internal_tick_as_midi_clock) {
        MIDI::sendRealTime(midi::Clock);
      }
      // Toggle the flag for the next tick
      _send_this_internal_tick_as_midi_clock = !_send_this_internal_tick_as_midi_clock;
    } else {
      _send_this_internal_tick_as_midi_clock =
          true; // Ensure it's ready if playback starts/config changes
    }
  }

  // Forward TempoEvent if the event source matches the current handler source.
  // This ensures we only process ticks from the authoritative source.
  if (event.source == current_source_) {
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
  if (current_source_ == ClockSource::INTERNAL && _playback_state == PlaybackState::STOPPED) {
    // When stopping, reset the flag so the first tick on resume (if conditions met) sends a clock.
    _send_this_internal_tick_as_midi_clock = true;
  }
}

void TempoHandler::update() {
  // This method is called from the main loop to handle auto-switching.
  if (current_source_ == ClockSource::INTERNAL) {
    // If we are on internal clock, check if MIDI clock has become active
    if (_midi_clock_processor_ref.get_derived_bpm() > 0.0f) {
      // printf("TempoHandler Update: MIDI clock detected. Switching to MIDI.\n");
      set_clock_source(ClockSource::MIDI);
    }
  } else if (current_source_ == ClockSource::MIDI) {
    // If we are on MIDI clock, check if it has timed out
    if (_midi_clock_processor_ref.get_derived_bpm() == 0.0f) {
      // printf("TempoHandler Update: MIDI clock timed out. Switching to INTERNAL.\n");
      set_clock_source(ClockSource::INTERNAL);
    }
  }
}

} // namespace musin::timing
