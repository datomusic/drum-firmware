#include "musin/timing/clock_router.h"
#include "pico/time.h"

namespace musin::timing {

ClockRouter::ClockRouter(InternalClock &internal_clock_ref,
                         MidiClockProcessor &midi_clock_processor_ref,
                         SyncIn &sync_in_ref, ClockSource initial_source)
    : internal_clock_(internal_clock_ref),
      midi_clock_processor_(midi_clock_processor_ref), sync_in_(sync_in_ref),
      current_source_(initial_source) {
  set_clock_source(initial_source);
}

void ClockRouter::notification(musin::timing::ClockEvent event) {
  // Forward the event as-is; sources already tag their origin
  notify_observers(event);
}

void ClockRouter::set_clock_source(ClockSource source) {
  if (initialized_ && source == current_source_) {
    return;
  }

  if (initialized_) {
    detach_current_source();
  }

  current_source_ = source;
  attach_source(source);
  initialized_ = true;
}

void ClockRouter::detach_current_source() {
  switch (current_source_) {
  case ClockSource::INTERNAL:
    internal_clock_.remove_observer(*this);
    internal_clock_.stop();
    break;
  case ClockSource::MIDI:
    midi_clock_processor_.remove_observer(*this);
    midi_clock_processor_.set_forward_echo_enabled(false);
    break;
  case ClockSource::EXTERNAL_SYNC:
    sync_in_.remove_observer(*this);
    break;
  }
}

void ClockRouter::attach_source(ClockSource source) {
  switch (source) {
  case ClockSource::INTERNAL:
    internal_clock_.add_observer(*this);
    internal_clock_.start();
    break;
  case ClockSource::MIDI:
    midi_clock_processor_.add_observer(*this);
    midi_clock_processor_.reset();
    midi_clock_processor_.set_forward_echo_enabled(true);
    break;
  case ClockSource::EXTERNAL_SYNC:
    sync_in_.add_observer(*this);
    break;
  }
}

} // namespace musin::timing
