#include "musin/timing/tempo_handler.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_event.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::sendRealTime
#include "drum/config.h"             // For drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER
#include "midi_Defs.h"               // For midi::Clock

// Include headers for specific clock types if needed for identification
// #include "internal_clock.h"
#include "musin/timing/midi_clock_processor.h" // Added include
// #include "external_sync_clock.h"

namespace musin::timing {

TempoHandler::TempoHandler(InternalClock &internal_clock_ref,
                           MidiClockProcessor &midi_clock_processor_ref,
                           ClockSource initial_source)
    : _internal_clock_ref(internal_clock_ref),
      _midi_clock_processor_ref(midi_clock_processor_ref), // Initialize new member
      current_source_(ClockSource::INTERNAL), // Initialize current_source_ before calling set_clock_source
      _playback_state(PlaybackState::STOPPED) {
  // Set the initial clock source, which will also handle starting the correct clock
  set_clock_source(initial_source);
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source == current_source_) {
    return;
  }

  // Stop the currently active clock source if necessary
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.stop();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.remove_observer(*this);
    _midi_clock_processor_ref.reset(); // Reset when switching away from MIDI
  }
  // Add logic for other clock sources (e.g., EXTERNAL_SYNC) if they are managed here

  current_source_ = source;

  // Start the new clock source if necessary
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.start();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.add_observer(*this); // Observe when MIDI is the source
    // MidiClockProcessor is driven by external MIDI ticks.
    // Ensure internal clock is stopped (handled above).
  }
  // Add logic for other clock sources (e.g., EXTERNAL_SYNC)
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  // Only process and forward ticks if they come from the currently selected source                                   
  if (event.source == ClockSource::INTERNAL && current_source_ == ClockSource::INTERNAL) {                                             
    // If internal clock is the master, send MIDI clock if playing OR                                                                  
    // if configured to send when stopped.                                                                                             
    if (_playback_state == PlaybackState::PLAYING ||                                                                                   
        drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER) {                                                                        
       MIDI::sendRealTime(midi::Clock);                                                                                                 
     }                                                                                                                                  
  }                                                                                                                                    
  // Forward TempoEvent if the event source matches the current handler source                                                         
  // This part is separate from MIDI clock sending.                                                                                    
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
}

} // namespace musin::timing
