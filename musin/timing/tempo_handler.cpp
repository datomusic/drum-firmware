#include "musin/timing/tempo_handler.h"
#include "midi_Defs.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/timing_constants.h"
#include "pico/time.h"

namespace musin::timing {

TempoHandler::TempoHandler(InternalClock &internal_clock_ref,
                           MidiClockProcessor &midi_clock_processor_ref,
                           SyncIn &sync_in_ref, ClockRouter &clock_router_ref,
                           SpeedAdapter &speed_adapter_ref,
                           bool send_midi_clock_when_stopped,
                           ClockSource initial_source)
    : _internal_clock_ref(internal_clock_ref),
      _midi_clock_processor_ref(midi_clock_processor_ref),
      _sync_in_ref(sync_in_ref), _clock_router_ref(clock_router_ref),
      _speed_adapter_ref(speed_adapter_ref), current_source_(initial_source),
      _playback_state(PlaybackState::STOPPED),
      current_speed_modifier_(SpeedModifier::NORMAL_SPEED), phase_24_(0),
      tick_count_(0),
      _send_midi_clock_when_stopped(send_midi_clock_when_stopped) {
  _speed_adapter_ref.add_observer(*this);
  set_clock_source(initial_source);
}

void TempoHandler::set_clock_source(ClockSource source) {
  // If already set and initialized, nothing to do
  if (source == current_source_ && initialized_) {
    return;
  }

  current_source_ = source;
  phase_24_ = 0; // Reset phase on source change
  external_align_to_12_next_ = (current_source_ == ClockSource::EXTERNAL_SYNC);
  // Clear any deferred anchoring state when switching sources
  pending_anchor_on_next_external_tick_ = false;
  pending_manual_resync_flag_ = false;
  last_external_tick_us_ = 0;
  last_external_tick_interval_us_ = 0;

  // Route raw 24 PPQN through ClockRouter
  _clock_router_ref.set_clock_source(current_source_);
  initialized_ = true;
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  if (event.source != current_source_) {
    return;
  }

  // Handle resync events immediately for all sources
  if (event.is_resync) {
    phase_24_ = 0; // Reset phase on resync
    if (current_source_ == ClockSource::EXTERNAL_SYNC) {
      external_align_to_12_next_ = true; // Next physical pulse aligns to 12
    }
    musin::timing::TempoEvent resync_tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = true};
    notify_observers(resync_tempo_event);
    return;
  }

  // Capture timing for MIDI look-behind anchoring
  if (current_source_ == ClockSource::MIDI) {
    uint32_t now_us =
        static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
    if (last_external_tick_us_ != 0) {
      last_external_tick_interval_us_ = now_us - last_external_tick_us_;
    }
    last_external_tick_us_ = now_us;

    // If a manual anchor was requested for MIDI, anchor on this tick
    if (pending_anchor_on_next_external_tick_) {
      tick_count_++;
      phase_24_ =
          external_align_to_12_next_ ? PHASE_EIGHTH_OFFBEAT : PHASE_DOWNBEAT;
      external_align_to_12_next_ = !external_align_to_12_next_;
      musin::timing::TempoEvent tempo_event{.tick_count = tick_count_,
                                            .phase_24 = phase_24_,
                                            .is_resync =
                                                pending_manual_resync_flag_};
      pending_anchor_on_next_external_tick_ = false;
      pending_manual_resync_flag_ = false;
      notify_observers(tempo_event);
      return;
    }
  }

  // External sync: anchor on physical pulses to 0/12, else advance normally
  if (current_source_ == ClockSource::EXTERNAL_SYNC &&
      event.is_physical_pulse) {
    tick_count_++;
    phase_24_ =
        external_align_to_12_next_ ? PHASE_EIGHTH_OFFBEAT : PHASE_DOWNBEAT;
    external_align_to_12_next_ = !external_align_to_12_next_;
    musin::timing::TempoEvent tempo_event{.tick_count = tick_count_,
                                          .phase_24 = phase_24_,
                                          .is_resync =
                                              pending_manual_resync_flag_};
    pending_manual_resync_flag_ = false;
    notify_observers(tempo_event);
    return;
  }

  // For all other ticks, advance phase and emit
  advance_phase_and_emit_event();
}

// Removed: speed scaling and tick selection is handled by SpeedAdapter

void TempoHandler::set_bpm(float bpm) {
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.set_bpm(bpm);
  }
}

void TempoHandler::set_speed_modifier(SpeedModifier modifier) {
  current_speed_modifier_ = modifier;
  _speed_adapter_ref.set_modifier(modifier);
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
                                                     .phase_24 = phase_24_,
                                                     .is_resync = true};
        notify_observers(resync_tempo_event);

        // No additional local advancement; SpeedAdapter governs cadence
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
    musin::timing::TempoEvent resync_tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = true};
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
  musin::timing::TempoEvent tempo_event{
      .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = false};

  notify_observers(tempo_event);
}

} // namespace musin::timing
