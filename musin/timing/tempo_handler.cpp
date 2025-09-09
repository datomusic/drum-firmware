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
      current_speed_modifier_(SpeedModifier::NORMAL_SPEED), phase_24_(0),
      tick_count_(0),
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
  phase_24_ = 0; // Reset phase on source change
  external_align_to_12_next_ = (current_source_ == ClockSource::EXTERNAL_SYNC);
  half_prescaler_toggle_ = false; // Reset prescaler on source change
  physical_pulse_counter_ = 0;    // Reset pulse counter on source change

  // Setup for the new source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.add_observer(*this);
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
      // Send MIDI realtime clock on every internal tick (24 PPQN)
      MIDI::sendRealTime(midi::Clock);
    }
  }

  if (event.source != current_source_) {
    return;
  }

  // Handle resync events immediately for all sources
  if (event.is_resync) {
    phase_24_ = 0; // Reset phase on resync
    if (current_source_ == ClockSource::EXTERNAL_SYNC) {
      external_align_to_12_next_ = true; // Next physical pulse aligns to 12
      half_prescaler_toggle_ = false;    // Reset prescaler on resync
      physical_pulse_counter_ = 0;       // Reset pulse counter on resync
    }
    musin::timing::TempoEvent resync_tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = true};
    notify_observers(resync_tempo_event);
    return;
  }

  // Always forward ticks regardless of playback state - device should always
  // send clocks
  // Simplified speed modifier processing for all external sources
  if (current_source_ == ClockSource::MIDI ||
      current_source_ == ClockSource::EXTERNAL_SYNC) {
    process_external_tick_with_speed_modifier(event.is_physical_pulse);
  } else {
    // Internal source: advance phase and emit event on every tick
    advance_phase_and_emit_event();
  }
}

void TempoHandler::process_external_tick_with_speed_modifier(
    bool is_physical_pulse) {
  // Count raw external tick
  tick_count_++;

  // Optional anchoring on physical SyncIn pulses
  bool anchored_this_tick = false;
  if (is_physical_pulse) {
    physical_pulse_counter_++;

    if (current_speed_modifier_ == SpeedModifier::HALF_SPEED) {
      // In half-speed, anchor on every second physical pulse to avoid jumps
      if ((physical_pulse_counter_ % 2) == 0) {
        phase_24_ = external_align_to_12_next_ ? 12 : 0;
        external_align_to_12_next_ = !external_align_to_12_next_;
        anchored_this_tick = true;
      }
    } else {
      // Normal/Double: anchor on every physical pulse
      phase_24_ = external_align_to_12_next_ ? 12 : 0;
      external_align_to_12_next_ = !external_align_to_12_next_;
      anchored_this_tick = true;
    }
  }

  // Determine step per raw tick according to speed modifier
  uint8_t step = 1;
  bool advance_this_tick = true;
  switch (current_speed_modifier_) {
  case SpeedModifier::NORMAL_SPEED:
    step = 1;
    advance_this_tick = true;
    break;
  case SpeedModifier::HALF_SPEED:
    // Advance every other raw tick
    half_prescaler_toggle_ = !half_prescaler_toggle_;
    advance_this_tick = half_prescaler_toggle_;
    step = advance_this_tick ? 1 : 0;
    break;
  case SpeedModifier::DOUBLE_SPEED:
    step = 2;
    advance_this_tick = true;
    break;
  }

  // Event/advance ordering to avoid phase ambiguity and races:
  // - On anchored ticks: emit the anchor phase (0/12), then advance.
  // - On non-anchored ticks: advance first (if applicable), then emit.
  if (anchored_this_tick) {
    // Emit anchored phase
    musin::timing::TempoEvent tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = false};
    notify_observers(tempo_event);

    // Advance after anchor if this tick should advance due to speed modifier
    if (advance_this_tick && step > 0) {
      phase_24_ = static_cast<uint8_t>((phase_24_ + step) % 24);
    }
  } else if (advance_this_tick && step > 0) {
    // Non-anchored: advance, then emit the new phase
    phase_24_ = static_cast<uint8_t>((phase_24_ + step) % 24);
    musin::timing::TempoEvent tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = false};
    notify_observers(tempo_event);
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
  // Reset prescaler to a known state when changing speed
  half_prescaler_toggle_ = false;
  // Reset physical pulse counter so HALF speed anchoring starts from
  // a consistent boundary after mode changes.
  physical_pulse_counter_ = 0;
}

void TempoHandler::set_playback_state(PlaybackState new_state) {
  _playback_state = new_state;
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
    if (_midi_clock_processor_ref.is_active()) {
      if (current_source_ != ClockSource::MIDI) {
        set_clock_source(ClockSource::MIDI);
      }
    } else {
      if (current_source_ != ClockSource::MIDI &&
          current_source_ != ClockSource::INTERNAL) {
        set_clock_source(ClockSource::INTERNAL);
      }
    }
  }
}

void TempoHandler::trigger_manual_sync() {
  if (current_source_ != ClockSource::INTERNAL) {
    phase_24_ = 0; // Reset phase on manual sync
    musin::timing::TempoEvent resync_tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = true};
    notify_observers(resync_tempo_event);
  }
}

void TempoHandler::advance_phase_and_emit_event() {
  // Advance the 24 PPQN phase counter (0-23)
  phase_24_ = (phase_24_ + 1) % 24;

  // Advance the running tick count
  tick_count_++;

  // Emit tempo event with current phase information
  musin::timing::TempoEvent tempo_event{
      .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = false};

  notify_observers(tempo_event);
}

} // namespace musin::timing
