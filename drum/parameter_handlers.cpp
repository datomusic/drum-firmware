#include "parameter_handlers.h"
#include "message_router.h"
#include "musin/timing/tempo_handler.h"
#include "sequencer_controller.h"
#include <cmath>

namespace drum {

// --- TempoParameterHandler ---
TempoParameterHandler::TempoParameterHandler(musin::timing::TempoHandler &tempo_handler,
                                             MessageRouter &message_router)
    : tempo_handler_(tempo_handler), message_router_(message_router) {
}

void TempoParameterHandler::handle_update(float normalized_value) {
  float bpm = config::analog_controls::MIN_BPM_ADJUST +
              normalized_value * (config::analog_controls::MAX_BPM_ADJUST -
                                  config::analog_controls::MIN_BPM_ADJUST);
  tempo_handler_.set_bpm(bpm);
  message_router_.set_parameter(drum::Parameter::TEMPO, normalized_value);
}

// --- PitchParameterHandler ---
PitchParameterHandler::PitchParameterHandler(MessageRouter &message_router, uint8_t track_index)
    : message_router_(message_router), track_index_(track_index) {
}

void PitchParameterHandler::handle_update(float normalized_value) {
  message_router_.set_parameter(drum::Parameter::PITCH, normalized_value, track_index_);
}

// --- VolumeParameterHandler ---
VolumeParameterHandler::VolumeParameterHandler(MessageRouter &message_router)
    : message_router_(message_router) {
}

void VolumeParameterHandler::handle_update(float normalized_value) {
  message_router_.set_parameter(drum::Parameter::VOLUME, normalized_value);
}

// --- SwingParameterHandler ---
SwingParameterHandler::SwingParameterHandler(DefaultSequencerController &sequencer_controller,
                                             MessageRouter &message_router)
    : sequencer_controller_(sequencer_controller), message_router_(message_router) {
}

void SwingParameterHandler::handle_update(float normalized_value) {
  float distance_from_center =
      fabsf(normalized_value - config::analog_controls::SWING_KNOB_CENTER_VALUE);

  uint8_t swing_percent = config::analog_controls::SWING_BASE_PERCENT +
                          static_cast<uint8_t>(distance_from_center *
                                               config::analog_controls::SWING_PERCENT_SENSITIVITY);

  bool delay_odd = (normalized_value > config::analog_controls::SWING_KNOB_CENTER_VALUE);
  sequencer_controller_.set_swing_target(delay_odd);
  sequencer_controller_.set_swing_percent(swing_percent);
  message_router_.set_parameter(drum::Parameter::SWING, normalized_value, 0);
}

// --- CrushParameterHandler ---
CrushParameterHandler::CrushParameterHandler(MessageRouter &message_router)
    : message_router_(message_router) {
}

void CrushParameterHandler::handle_update(float normalized_value) {
  message_router_.set_parameter(drum::Parameter::CRUSH_EFFECT, normalized_value);
}

// --- RandomParameterHandler ---
RandomParameterHandler::RandomParameterHandler(DefaultSequencerController &sequencer_controller,
                                               MessageRouter &message_router)
    : sequencer_controller_(sequencer_controller), message_router_(message_router) {
}

void RandomParameterHandler::handle_update(float normalized_value) {
  bool was_active = sequencer_controller_.is_random_active();
  bool should_be_active =
      (normalized_value >= config::analog_controls::RANDOM_ACTIVATION_THRESHOLD);

  if (should_be_active && !was_active) {
    sequencer_controller_.activate_random();
  } else if (!should_be_active && was_active) {
    sequencer_controller_.deactivate_random();
  }
  sequencer_controller_.set_random_probability(normalized_value * 33);
  message_router_.set_parameter(drum::Parameter::RANDOM_EFFECT, normalized_value, 0);
}

// --- RepeatParameterHandler ---
RepeatParameterHandler::RepeatParameterHandler(DefaultSequencerController &sequencer_controller,
                                               MessageRouter &message_router)
    : sequencer_controller_(sequencer_controller), message_router_(message_router) {
}

void RepeatParameterHandler::handle_update(float normalized_value) {
  std::optional<uint32_t> intended_length = std::nullopt;
  if (normalized_value >= config::analog_controls::REPEAT_MODE_2_THRESHOLD) {
    intended_length = config::analog_controls::REPEAT_LENGTH_MODE_2;
  } else if (normalized_value >= config::analog_controls::REPEAT_MODE_1_THRESHOLD) {
    intended_length = config::analog_controls::REPEAT_LENGTH_MODE_1;
  }

  sequencer_controller_.set_intended_repeat_state(intended_length);
  message_router_.set_parameter(drum::Parameter::REPEAT_EFFECT, normalized_value);
}

// --- FilterParameterHandler ---
FilterParameterHandler::FilterParameterHandler(float &target_value) : target_value_(target_value) {
}

void FilterParameterHandler::handle_update(float normalized_value) {
  target_value_ = normalized_value;
}

} // namespace drum
