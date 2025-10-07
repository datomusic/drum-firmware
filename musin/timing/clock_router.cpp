#include "musin/timing/clock_router.h"
#include "musin/timing/sync_out.h"
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

  if (awaiting_first_tick_after_switch_ && event.source == current_source_) {
    awaiting_first_tick_after_switch_ = false;
  }
}

void ClockRouter::set_clock_source(ClockSource source) {
  if (initialized_ && source == current_source_) {
    return;
  }

  ClockSource old_source = current_source_;
  bool was_initialized = initialized_;

  if (initialized_) {
    detach_current_source();
  }

  current_source_ = source;
  attach_source(source);
  initialized_ = true;

  // Notify listener after source is fully changed
  if (source_change_listener_) {
    source_change_listener_->on_clock_source_changed(old_source, source);
  }

  // Emit resync event when switching to non-EXTERNAL_SYNC sources
  // to immediately reset phase and clear any pending state
  if (was_initialized && source != ClockSource::EXTERNAL_SYNC) {
    ClockEvent resync_event{source};
    resync_event.is_resync = true;
    notify_observers(resync_event);
  }
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
    awaiting_first_tick_after_switch_ = false;
    break;
  case ClockSource::MIDI:
    midi_clock_processor_.add_observer(*this);
    midi_clock_processor_.reset();
    midi_clock_processor_.set_forward_echo_enabled(true);
    awaiting_first_tick_after_switch_ = true;
    break;
  case ClockSource::EXTERNAL_SYNC:
    sync_in_.add_observer(*this);
    awaiting_first_tick_after_switch_ = true;
    break;
  }
}

void ClockRouter::set_bpm(float bpm) {
  if (current_source_ == ClockSource::INTERNAL) {
    internal_clock_.set_bpm(bpm);
  }
}

void ClockRouter::trigger_resync() {
  if (current_source_ == ClockSource::INTERNAL) {
    internal_clock_.reset();
  }

  ClockEvent resync_event{current_source_};
  resync_event.is_resync = true;
  notify_observers(resync_event);

  if (sync_out_ != nullptr) {
    sync_out_->resync();
  }
}

void ClockRouter::set_sync_out(SyncOut *sync_out_ptr) {
  sync_out_ = sync_out_ptr;
}

void ClockRouter::resync_sync_output() {
  if (sync_out_ != nullptr) {
    sync_out_->resync();
  }
}

void ClockRouter::update_auto_source_switching() {
  if (!auto_switching_enabled_) {
    return;
  }

  if (sync_in_.is_cable_connected()) {
    if (current_source_ != ClockSource::EXTERNAL_SYNC) {
      set_clock_source(ClockSource::EXTERNAL_SYNC);
    }
    return;
  }

  const bool awaiting_midi_tick =
      awaiting_first_tick_after_switch_ && current_source_ == ClockSource::MIDI;

  if (current_source_ == ClockSource::MIDI) {
    return;
  }

  if (midi_clock_processor_.is_active() || awaiting_midi_tick) {
    if (current_source_ != ClockSource::MIDI) {
      set_clock_source(ClockSource::MIDI);
    }
  } else {
    if (current_source_ != ClockSource::INTERNAL) {
      set_clock_source(ClockSource::INTERNAL);
    }
  }
}

void ClockRouter::set_auto_switching_enabled(bool enabled) {
  auto_switching_enabled_ = enabled;
}

void ClockRouter::set_source_change_listener(ISourceChangeListener *listener) {
  source_change_listener_ = listener;
}

} // namespace musin::timing
