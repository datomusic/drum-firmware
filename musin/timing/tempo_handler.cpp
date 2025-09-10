#include "musin/timing/tempo_handler.h"
#include "midi_Defs.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/timing_constants.h"
#include "pico/time.h"

namespace musin::timing {

namespace {
// Normalize any integer to [0, DEFAULT_PPQN-1] for 24 PPQN phase safety.
constexpr uint8_t wrap24(int v) noexcept {
  int r = v % static_cast<int>(musin::timing::DEFAULT_PPQN);
  return static_cast<uint8_t>(
      r < 0 ? r + static_cast<int>(musin::timing::DEFAULT_PPQN) : r);
}
} // namespace

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
  // If already set and initialized, nothing to do
  if (source == current_source_ && initialized_) {
    return;
  }

  // Cleanup for the old source only if we had previously initialized
  if (initialized_) {
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
  }

  current_source_ = source;
  phase_24_ = 0; // Reset phase on source change
  external_align_to_12_next_ = (current_source_ == ClockSource::EXTERNAL_SYNC);
  half_prescaler_toggle_ = false; // Reset prescaler on source change
  physical_pulse_counter_ = 0;    // Reset pulse counter on source change
  // Clear any deferred anchoring state when switching sources
  pending_anchor_on_next_external_tick_ = false;
  pending_manual_resync_flag_ = false;
  last_external_tick_us_ = 0;
  last_external_tick_interval_us_ = 0;

  // Setup for the new source
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.add_observer(*this);
    _internal_clock_ref.start();
  } else if (current_source_ == ClockSource::MIDI) {
    _midi_clock_processor_ref.add_observer(*this);
  } else if (current_source_ == ClockSource::EXTERNAL_SYNC) {
    _clock_multiplier_ref.add_observer(*this);
  }
  initialized_ = true;
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
    musin::timing::TempoEvent resync_tempo_event{.tick_count = tick_count_,
                                                 .phase_24 = wrap24(phase_24_),
                                                 .is_resync = true};
    notify_observers(resync_tempo_event);
    return;
  }

  // Always forward ticks regardless of playback state - device should always
  // send clocks
  // Simplified speed modifier processing for all external sources
  if (current_source_ == ClockSource::MIDI ||
      current_source_ == ClockSource::EXTERNAL_SYNC) {
    // Capture timing for MIDI look-behind anchoring
    if (current_source_ == ClockSource::MIDI) {
      uint32_t now_us =
          static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
      if (last_external_tick_us_ != 0) {
        last_external_tick_interval_us_ =
            now_us - last_external_tick_us_; // wrap-safe
      }
      last_external_tick_us_ = now_us;
    }
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
  bool anchored_is_manual_resync = false;

  // If a manual anchor was requested while MIDI is the source, perform it on
  // the first subsequent external tick. This produces an anchor at the nearest
  // upcoming MIDI clock boundary rather than immediately.
  if (pending_anchor_on_next_external_tick_ &&
      current_source_ == ClockSource::MIDI) {
    phase_24_ =
        external_align_to_12_next_ ? PHASE_EIGHTH_OFFBEAT : PHASE_DOWNBEAT;
    external_align_to_12_next_ = !external_align_to_12_next_;
    anchored_this_tick = true;
    anchored_is_manual_resync = pending_manual_resync_flag_;
    pending_manual_resync_flag_ = false;
    pending_anchor_on_next_external_tick_ = false;
  }
  if (is_physical_pulse) {
    physical_pulse_counter_++;

    if (current_speed_modifier_ == SpeedModifier::HALF_SPEED) {
      // In half-speed, anchor on every second physical pulse to avoid jumps
      if ((physical_pulse_counter_ % 2) == 0) {
        phase_24_ =
            external_align_to_12_next_ ? PHASE_EIGHTH_OFFBEAT : PHASE_DOWNBEAT;
        external_align_to_12_next_ = !external_align_to_12_next_;
        anchored_this_tick = true;
      }
    } else {
      // Normal/Double: anchor on every physical pulse
      phase_24_ =
          external_align_to_12_next_ ? PHASE_EIGHTH_OFFBEAT : PHASE_DOWNBEAT;
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
    musin::timing::TempoEvent tempo_event{.tick_count = tick_count_,
                                          .phase_24 = wrap24(phase_24_),
                                          .is_resync =
                                              anchored_is_manual_resync};
    notify_observers(tempo_event);

    // Advance after anchor if this tick should advance due to speed modifier
    if (advance_this_tick && step > 0) {
      phase_24_ = static_cast<uint8_t>((phase_24_ + step) %
                                       musin::timing::DEFAULT_PPQN);
    }
  } else if (advance_this_tick && step > 0) {
    // Non-anchored: advance, then emit the new phase
    phase_24_ =
        static_cast<uint8_t>((phase_24_ + step) % musin::timing::DEFAULT_PPQN);
    musin::timing::TempoEvent tempo_event{.tick_count = tick_count_,
                                          .phase_24 = wrap24(phase_24_),
                                          .is_resync = false};
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

  // Ensure even parity when entering DOUBLE speed under external clock sources
  // so that phases PHASE_DOWNBEAT and PHASE_EIGHTH_OFFBEAT remain reachable
  // when stepping by 2.
  if (modifier == SpeedModifier::DOUBLE_SPEED &&
      current_source_ != ClockSource::INTERNAL) {
    if ((phase_24_ & 1u) != 0u) {
      phase_24_ =
          static_cast<uint8_t>((phase_24_ + 1u) % musin::timing::DEFAULT_PPQN);
    }
  }
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
  switch (current_source_) {
  case ClockSource::INTERNAL:
    // No-op for internal clock; phase advances locally.
    break;
  case ClockSource::MIDI:
    // Attempt look-behind: if the press is shortly after a MIDI tick, treat
    // that last tick as the anchor boundary (0/12). Otherwise, defer to the
    // next tick.
    {
      uint32_t now_us =
          static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
      bool have_last_tick = (last_external_tick_us_ != 0);
      // Define a window as half of the last tick interval, with a reasonable
      // upper bound to avoid overly long windows at slow tempos.
      uint32_t interval_us = last_external_tick_interval_us_;
      if (interval_us == 0 && have_last_tick) {
        // Fallback estimate: assume 120 BPM -> ~20.8ms per 24ppqn tick
        interval_us = 20833u;
      }
      uint32_t window_us = interval_us / 2u;
      if (window_us > 12000u) {
        window_us = 12000u; // cap at 12ms
      }

      if (have_last_tick && (now_us - last_external_tick_us_) <= window_us) {
        // Backdate anchor to the last MIDI tick: emit resync at anchor phase
        // (0 or 12), then advance locally if that tick would have advanced.
        uint8_t anchor_phase =
            external_align_to_12_next_ ? PHASE_EIGHTH_OFFBEAT : PHASE_DOWNBEAT;

        // After an anchor we flip the 0/12 toggle.
        external_align_to_12_next_ = !external_align_to_12_next_;

        // Emit resync at the anchor phase first
        phase_24_ = anchor_phase;
        musin::timing::TempoEvent resync_tempo_event{.tick_count = tick_count_,
                                                     .phase_24 =
                                                         wrap24(phase_24_),
                                                     .is_resync = true};
        notify_observers(resync_tempo_event);

        // Determine whether the last tick would have advanced based on current
        // scaling state. For HALF speed, the decision for the last tick is the
        // current half_prescaler_toggle_ value (it toggles per tick).
        bool would_advance = true;
        uint8_t step = 1;
        switch (current_speed_modifier_) {
        case SpeedModifier::NORMAL_SPEED:
          step = 1;
          would_advance = true;
          break;
        case SpeedModifier::HALF_SPEED:
          would_advance = half_prescaler_toggle_;
          step = would_advance ? 1 : 0;
          break;
        case SpeedModifier::DOUBLE_SPEED:
          step = 2;
          would_advance = true;
          break;
        }

        // Advance local phase as if that anchored tick had completed
        if (would_advance && step > 0) {
          phase_24_ = static_cast<uint8_t>((phase_24_ + step) %
                                           musin::timing::DEFAULT_PPQN);
        }
      } else {
        // Defer anchoring to the next incoming MIDI tick
        pending_anchor_on_next_external_tick_ = true;
        pending_manual_resync_flag_ = true; // mark the anchored tick as resync
      }
    }
    break;
  case ClockSource::EXTERNAL_SYNC:
    // Immediate resync for SyncIn
    phase_24_ = PHASE_DOWNBEAT; // Reset phase on manual sync
    musin::timing::TempoEvent resync_tempo_event{.tick_count = tick_count_,
                                                 .phase_24 = wrap24(phase_24_),
                                                 .is_resync = true};
    notify_observers(resync_tempo_event);
    break;
  }
}

void TempoHandler::advance_phase_and_emit_event() {
  // Advance the 24 PPQN phase counter (0..DEFAULT_PPQN-1)
  phase_24_ = (phase_24_ + 1) % musin::timing::DEFAULT_PPQN;

  // Advance the running tick count
  tick_count_++;

  // Emit tempo event with current phase information
  musin::timing::TempoEvent tempo_event{.tick_count = tick_count_,
                                        .phase_24 = wrap24(phase_24_),
                                        .is_resync = false};

  notify_observers(tempo_event);
}

} // namespace musin::timing
