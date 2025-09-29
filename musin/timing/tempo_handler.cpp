#include "musin/timing/tempo_handler.h"
#include "drum/config.h"
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

  // Route raw 24 PPQN through ClockRouter
  _clock_router_ref.set_clock_source(current_source_);

  // Re-evaluate tempo knob position for the new clock source
  // This fixes issue #486: tempo knob position is now applied when switching
  // sources
  set_tempo_control_value(last_tempo_knob_value_);

  initialized_ = true;
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

uint8_t TempoHandler::calculate_aligned_phase() const {
  constexpr uint8_t half_cycle = musin::timing::DEFAULT_PPQN / 2;
  constexpr uint8_t quarter_cycle = musin::timing::DEFAULT_PPQN / 4;

  // Use quarter-cycle thresholds so we pick the closest half-cycle anchor
  // without large backward jumps near wrap-around.
  uint8_t target_phase = 0;
  if (phase_24_ >= quarter_cycle &&
      phase_24_ < static_cast<uint8_t>(half_cycle + quarter_cycle)) {
    target_phase = half_cycle;
  }
  return target_phase;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  if (event.source != current_source_) {
    return;
  }

  // Always realign on external physical pulses
  if (event.source == ClockSource::EXTERNAL_SYNC && event.is_physical_pulse) {
    event.anchor_to_phase = calculate_aligned_phase();
  }

  if (event.is_resync) {
    // Handle resync: set phase to anchor or 0, increment tick, emit resync
    // event, return
    phase_24_ = (event.anchor_to_phase != ClockEvent::ANCHOR_PHASE_NONE)
                    ? event.anchor_to_phase
                    : 0;
    tick_count_++;
    musin::timing::TempoEvent tempo_event{
        .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = true};
    notify_observers(tempo_event);
    return;
  }

  // Handle normal tick: advance phase normally or honor anchor if present
  uint8_t next_phase = (event.anchor_to_phase != ClockEvent::ANCHOR_PHASE_NONE)
                           ? event.anchor_to_phase
                           : (phase_24_ + 1) % musin::timing::DEFAULT_PPQN;

  tick_count_++;
  phase_24_ = next_phase;
  musin::timing::TempoEvent tempo_event{
      .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = false};
  notify_observers(tempo_event);
}

// Removed: speed scaling and tick selection is handled by SpeedAdapter

void TempoHandler::set_bpm(float bpm) {
  if (current_source_ == ClockSource::INTERNAL) {
    _internal_clock_ref.set_bpm(bpm);
  }
}

void TempoHandler::set_speed_modifier(SpeedModifier modifier) {
  current_speed_modifier_ = modifier;

  // Route speed changes to individual external clock sources
  _sync_in_ref.set_speed_modifier(modifier);
  _midi_clock_processor_ref.set_speed_modifier(modifier);

  // Handle SpeedAdapter: force to NORMAL_SPEED for HALF_SPEED to prevent double
  // modification, otherwise pass through for DOUBLE_SPEED or other cases
  if (modifier == SpeedModifier::HALF_SPEED) {
    // External sources handle half-speed internally, ensure SpeedAdapter
    // doesn't also halve
    _speed_adapter_ref.set_modifier(SpeedModifier::NORMAL_SPEED);
  } else {
    // For DOUBLE_SPEED or NORMAL_SPEED, pass through to SpeedAdapter
    _speed_adapter_ref.set_modifier(modifier);
  }
}

void TempoHandler::set_tempo_control_value(float knob_value) {
  // Store the knob value for re-evaluation on clock source changes
  last_tempo_knob_value_ = knob_value;

  if (current_source_ == ClockSource::INTERNAL) {
    // Internal clock: knob controls BPM
    float bpm = drum::config::analog_controls::MIN_BPM_ADJUST +
                knob_value * (drum::config::analog_controls::MAX_BPM_ADJUST -
                              drum::config::analog_controls::MIN_BPM_ADJUST);
    set_bpm(bpm);
  } else {
    // External clock: knob controls speed modifier
    SpeedModifier modifier = SpeedModifier::NORMAL_SPEED;
    if (knob_value < 0.1f) {
      modifier = SpeedModifier::HALF_SPEED;
    } else if (knob_value > 0.9f) {
      modifier = SpeedModifier::DOUBLE_SPEED;
    }
    set_speed_modifier(modifier);
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
    if (!drum::config::RETRIGGER_SYNC_ON_PLAYBUTTON) {
      break;
    }

    // Reset internal clock timing so the next automatic tick lands one
    // interval after the manual downbeat we emit below.
    _internal_clock_ref.resync();

    // Emit an immediate resync event directly
    emit_manual_resync_event(calculate_aligned_phase());
    break;
  case ClockSource::MIDI:
    if (drum::config::RETRIGGER_SYNC_ON_PLAYBUTTON) {
      // Emit immediate resync event directly
      emit_manual_resync_event(calculate_aligned_phase());
    }
    break;
  case ClockSource::EXTERNAL_SYNC:
    // No immediate realignment - respect external timing reference
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

void TempoHandler::emit_manual_resync_event(uint8_t anchor_phase) {
  // Set phase to anchor and increment tick count
  phase_24_ = anchor_phase;
  tick_count_++;

  // Emit resync tempo event
  musin::timing::TempoEvent tempo_event{
      .tick_count = tick_count_, .phase_24 = phase_24_, .is_resync = true};
  notify_observers(tempo_event);
}

} // namespace musin::timing
