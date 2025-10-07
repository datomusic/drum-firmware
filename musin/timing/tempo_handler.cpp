#include "musin/timing/tempo_handler.h"
#include "drum/config.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/timing_constants.h"

namespace musin::timing {

TempoHandler::TempoHandler(ClockRouter &clock_router_ref,
                           SpeedAdapter &speed_adapter_ref,
                           bool send_midi_clock_when_stopped,
                           ClockSource initial_source)
    : _clock_router_ref(clock_router_ref),
      _speed_adapter_ref(speed_adapter_ref),
      _playback_state(PlaybackState::STOPPED),
      current_speed_modifier_(SpeedModifier::NORMAL_SPEED), phase_12_(0),
      tick_count_(0),
      _send_midi_clock_when_stopped(send_midi_clock_when_stopped) {
  _speed_adapter_ref.add_observer(*this);
  set_clock_source(initial_source);
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source == _clock_router_ref.get_clock_source() && initialized_) {
    return;
  }

  phase_12_ = 0; // Reset phase on source change
  _clock_router_ref.set_clock_source(source);

  // When switching to internal clock, the speed modifier should not apply.
  // Reset it to normal to avoid carrying over double/half speed from an
  // external source.
  if (source == ClockSource::INTERNAL) {
    set_speed_modifier(SpeedModifier::NORMAL_SPEED);
  }

  // Re-evaluate tempo knob position for the new clock source
  // This fixes issue #486: tempo knob position is now applied when switching
  // sources
  set_tempo_control_value(last_tempo_knob_value_);
  initialized_ = true;
}

ClockSource TempoHandler::get_clock_source() const {
  return _clock_router_ref.get_clock_source();
}

uint8_t TempoHandler::calculate_aligned_phase() const {
  switch (current_speed_modifier_) {
  case SpeedModifier::HALF_SPEED: {
    // Align to quarter-note grid (0, 3, 6, 9)
    constexpr uint8_t alignment_lut[12] = {0, 0, 3, 3, 3, 6, 6, 6, 9, 9, 9, 0};
    return alignment_lut[phase_12_];
  }
  case SpeedModifier::NORMAL_SPEED: {
    // Align to eighth-note grid (0, 6)
    constexpr uint8_t alignment_lut[12] = {0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6};
    return alignment_lut[phase_12_];
  }
  case SpeedModifier::DOUBLE_SPEED:
    // Always align to downbeat (0)
    return 0;
  }
  return 0;
}

void TempoHandler::notification(musin::timing::ClockEvent event) {
  constexpr uint8_t NO_ANCHOR = 0xFF;
  uint8_t anchor_phase = NO_ANCHOR;

  if (event.source == ClockSource::EXTERNAL_SYNC && event.is_downbeat) {
    anchor_phase = calculate_aligned_phase();
    waiting_for_external_downbeat_ = false;
  }

  if (waiting_for_external_downbeat_) {
    return;
  }

  if (event.is_resync) {
    phase_12_ = (anchor_phase != NO_ANCHOR) ? anchor_phase : 0;
    tick_count_++;
    musin::timing::TempoEvent tempo_event{.phase_12 = phase_12_,
                                          .is_resync = true};
    notify_observers(tempo_event);
    return;
  }

  uint8_t next_phase = (anchor_phase != NO_ANCHOR)
                           ? anchor_phase
                           : (phase_12_ + 1) % musin::timing::DEFAULT_PPQN;

  tick_count_++;
  phase_12_ = next_phase;
  musin::timing::TempoEvent tempo_event{.phase_12 = phase_12_,
                                        .is_resync = false};
  notify_observers(tempo_event);
}

void TempoHandler::set_bpm(float bpm) {
  _clock_router_ref.set_bpm(bpm);
}

void TempoHandler::set_speed_modifier(SpeedModifier modifier) {
  current_speed_modifier_ = modifier;
  _speed_adapter_ref.set_modifier(modifier);
}

SpeedModifier TempoHandler::get_speed_modifier() const {
  return current_speed_modifier_;
}

PlaybackState TempoHandler::get_playback_state() const {
  return _playback_state;
}

void TempoHandler::set_tempo_control_value(float knob_value) {
  last_tempo_knob_value_ = knob_value;

  if (get_clock_source() == ClockSource::INTERNAL) {
    float bpm = drum::config::analog_controls::MIN_BPM_ADJUST +
                knob_value * (drum::config::analog_controls::MAX_BPM_ADJUST -
                              drum::config::analog_controls::MIN_BPM_ADJUST);
    set_bpm(bpm);
    // Ensure speed modifier is always normal when on internal clock
    set_speed_modifier(SpeedModifier::NORMAL_SPEED);
  } else {
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

void TempoHandler::trigger_manual_sync(uint8_t target_phase) {
  if (!drum::config::RETRIGGER_SYNC_ON_PLAYBUTTON) {
    return;
  }

  switch (get_clock_source()) {
  case ClockSource::INTERNAL:
  case ClockSource::MIDI:
    _clock_router_ref.trigger_resync();
    emit_manual_resync_event(target_phase);
    break;
  case ClockSource::EXTERNAL_SYNC:
    waiting_for_external_downbeat_ = true;
    break;
  }
}

void TempoHandler::emit_manual_resync_event(uint8_t anchor_phase) {
  phase_12_ = anchor_phase;
  tick_count_++;
  musin::timing::TempoEvent tempo_event{.phase_12 = phase_12_,
                                        .is_resync = true};
  notify_observers(tempo_event);
}

} // namespace musin::timing
