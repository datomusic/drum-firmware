#include "pizza_controls.h"
#include "drum/drumpad_factory.h"
#include "drum/ui/pizza_display.h"
#include "message_router.h"
#include "musin/hal/analog_mux_scanner.h"
#include "musin/hal/logger.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "pico/time.h"
#include "sequencer_controller.h"
#include "system_state_machine.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace drum {

using musin::ui::AnalogControl;
using musin::ui::Drumpad;

PizzaControls::PizzaControls(
    drum::PizzaDisplay &display_ref,
    musin::timing::TempoHandler &tempo_handler_ref,
    drum::DefaultSequencerController &sequencer_controller_ref,
    drum::MessageRouter &message_router_ref,
    drum::SystemStateMachine &system_state_machine_ref,
    musin::Logger &logger_ref)
    : display(display_ref), _tempo_handler_ref(tempo_handler_ref),
      _sequencer_controller_ref(sequencer_controller_ref),
      _message_router_ref(message_router_ref),
      _system_state_machine_ref(system_state_machine_ref),
      _logger_ref(logger_ref),
      _scanner(PIZZA_MUX_ADC_PIN, analog_address_pins), // Initialize scanner
      keypad_component(this), drumpad_component(this), analog_component(this),
      playbutton_component(this) {
}

void PizzaControls::init() {
  if (is_control_panel_disconnected(_logger_ref)) {
    _logger_ref.warn("Control panel appears disconnected (address pins "
                     "floating). Disabling local control.");
    _message_router_ref.set_local_control_mode(drum::LocalControlMode::OFF);
  } else {
    _logger_ref.info("Control panel detected. Local control enabled.");
  }

  _scanner.init();
  keypad_component.init();
  drumpad_component.init();
  analog_component.init();
  playbutton_component.init();
}

void PizzaControls::update(absolute_time_t now) {
  if (_message_router_ref.get_local_control_mode() ==
      drum::LocalControlMode::ON) {
    _scanner.scan(); // Scan all analog inputs at once

    keypad_component.update();
    drumpad_component.update();
    analog_component.update(now);
    playbutton_component.update();
  }
}

bool PizzaControls::is_running() const {
  return _sequencer_controller_ref.is_running();
}

// --- KeypadComponent ---
PizzaControls::KeypadComponent::KeypadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      keypad(keypad_decoder_pins, keypad_columns_pins,
             config::keypad::POLL_INTERVAL_MS, config::keypad::DEBOUNCE_TIME_MS,
             config::keypad::HOLD_TIME_MS, config::keypad::TAP_TIME_MS),
      keypad_observer(this, keypad_cc_map, config::keypad::_CHANNEL) {
}

void PizzaControls::KeypadComponent::init() {
  keypad.init();
  keypad.add_observer(keypad_observer);
}

void PizzaControls::KeypadComponent::update() {
  keypad.scan();
}

void PizzaControls::KeypadComponent::KeypadEventHandler::handle_sample_select(
    musin::ui::KeypadEvent event) {
  PizzaControls *controls = parent->parent_controls;

  if (event.type == musin::ui::KeypadEvent::Type::Press) {
    uint8_t pad_index = 0;
    int8_t offset = 0;
    switch (event.row) {
    case 0:
      pad_index = 3;
      offset = -1;
      break;
    case 1:
      pad_index = 3;
      offset = 1;
      break;
    case 2:
      pad_index = 2;
      offset = -1;
      break;
    case 3:
      pad_index = 2;
      offset = 1;
      break;
    case 4:
      pad_index = 1;
      offset = -1;
      break;
    case 5:
      pad_index = 1;
      offset = 1;
      break;
    case 6:
      pad_index = 0;
      offset = -1;
      break;
    case 7:
      pad_index = 0;
      offset = 1;
      break;
    }
    controls->drumpad_component.select_note_for_pad(pad_index, offset);
    if (!controls->is_running()) {
      uint8_t note_to_play =
          controls->drumpad_component.get_note_for_pad(pad_index);
      controls->_sequencer_controller_ref.trigger_note_on(
          pad_index, note_to_play, config::keypad::PREVIEW_NOTE_VELOCITY);
    }
  }
}

void PizzaControls::KeypadComponent::KeypadEventHandler::handle_sequencer_step(
    musin::ui::KeypadEvent event) {
  PizzaControls *controls = parent->parent_controls;
  uint8_t track_index =
      (drum::PizzaDisplay::SEQUENCER_TRACKS_DISPLAYED - 1) - event.col;
  uint8_t step_index = (KEYPAD_ROWS - 1) - event.row;
  auto &track = controls->_sequencer_controller_ref.get_sequencer().get_track(
      track_index);

  if (event.type == musin::ui::KeypadEvent::Type::Press) {
    const bool now_enabled = track.toggle_step_enabled(step_index);
    if (now_enabled) {
      const uint8_t step_velocity = config::keypad::DEFAULT_STEP_VELOCITY;
      uint8_t note = controls->drumpad_component.get_note_for_pad(track_index);
      track.set_step_note(step_index, note);
      track.set_step_velocity(step_index, step_velocity);
      if (!controls->is_running()) {
        controls->_sequencer_controller_ref.trigger_note_on(track_index, note,
                                                            step_velocity);
      }
    }
  }
  if (event.type == musin::ui::KeypadEvent::Type::Hold) {
    if (!track.get_step(step_index).enabled) {
      track.set_step_enabled(step_index, true);
    }
    track.set_step_velocity(step_index, config::keypad::STEP_VELOCITY_ON_HOLD);
  }
}

void PizzaControls::KeypadComponent::KeypadEventHandler::notification(
    musin::ui::KeypadEvent event) {
  if (event.col >= config::keypad::SAMPLE_SELECT_START_COLUMN) {
    handle_sample_select(event);
  } else {
    handle_sequencer_step(event);
  }
}

// --- DrumpadComponent ---
PizzaControls::DrumpadComponent::DrumpadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr), drumpads(DrumpadFactory::create_drumpads()),
      drumpad_observer{this, parent_ptr->_logger_ref} {
}

void PizzaControls::DrumpadComponent::init() {
  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].init();
    drumpads[i].add_observer(drumpad_observer);
  }
}

void PizzaControls::DrumpadComponent::update() {
  PizzaControls *controls = parent_controls;
  for (size_t i = 0; i < drumpads.size(); ++i) {
    uint16_t raw_value = controls->_scanner.get_raw_value(
        drumpad_addresses[(drumpads[i].get_id())]);
    drumpads[i].update(raw_value);

    musin::ui::RetriggerMode current_mode = drumpads[i].get_retrigger_mode();
    if (current_mode != _last_known_retrigger_mode_per_pad[i]) {
      if (current_mode == musin::ui::RetriggerMode::Single) {
        controls->_sequencer_controller_ref.activate_play_on_every_step(
            static_cast<uint8_t>(i), 1);
      } else if (current_mode == musin::ui::RetriggerMode::Double) {
        controls->_sequencer_controller_ref.activate_play_on_every_step(
            static_cast<uint8_t>(i), 2);
      } else {
        controls->_sequencer_controller_ref.deactivate_play_on_every_step(
            static_cast<uint8_t>(i));
      }
    }
    _last_known_retrigger_mode_per_pad[i] = current_mode;
  }
}

void PizzaControls::DrumpadComponent::select_note_for_pad(uint8_t pad_index,
                                                          int8_t offset) {
  if (pad_index >= config::track_note_ranges.size())
    return;
  const auto &notes_for_pad = config::track_note_ranges[pad_index];
  if (notes_for_pad.empty())
    return;

  uint8_t current_note =
      parent_controls->_sequencer_controller_ref.get_active_note_for_track(
          pad_index);
  size_t num_notes_in_list = notes_for_pad.size();
  int32_t current_list_idx = -1;

  for (size_t i = 0; i < num_notes_in_list; ++i) {
    if (notes_for_pad[i] == current_note) {
      current_list_idx = static_cast<int32_t>(i);
      break;
    }
  }
  if (current_list_idx == -1)
    current_list_idx = 0;

  int32_t new_list_idx =
      (current_list_idx + offset + num_notes_in_list) % num_notes_in_list;
  uint8_t new_selected_note_value =
      notes_for_pad[static_cast<size_t>(new_list_idx)];

  parent_controls->_sequencer_controller_ref.set_active_note_for_track(
      pad_index, new_selected_note_value);
  parent_controls->_sequencer_controller_ref.get_sequencer()
      .get_track(pad_index)
      .set_note(new_selected_note_value);
}

uint8_t
PizzaControls::DrumpadComponent::get_note_for_pad(uint8_t pad_index) const {
  if (pad_index >= config::NUM_DRUMPADS)
    return config::drumpad::DEFAULT_FALLBACK_NOTE;
  return parent_controls->_sequencer_controller_ref.get_active_note_for_track(
      pad_index);
}

void PizzaControls::DrumpadComponent::DrumpadEventHandler::notification(
    musin::ui::DrumpadEvent event) {
  logger.debug("Drumpad ", static_cast<uint32_t>(event.pad_index));
  auto &seq_controller = parent->parent_controls->_sequencer_controller_ref;
  if (event.velocity.has_value()) {
    logger.debug("Velocity ", static_cast<uint32_t>(event.velocity.value()));
  }
  if (event.pad_index < config::NUM_DRUMPADS) {
    if (event.type == musin::ui::DrumpadEvent::Type::Press) {
      logger.debug("PRESSED ", static_cast<uint32_t>(event.pad_index));
      seq_controller.set_pad_pressed_state(event.pad_index, true);
      if (event.velocity.has_value()) {
        uint8_t note = parent->get_note_for_pad(event.pad_index);
        uint8_t velocity = event.velocity.value();
        seq_controller.trigger_note_on(event.pad_index, note, velocity);
      }
    } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
      logger.debug("RELEASED ", static_cast<uint32_t>(event.pad_index));
      seq_controller.set_pad_pressed_state(event.pad_index, false);
      uint8_t note = parent->get_note_for_pad(event.pad_index);
      seq_controller.trigger_note_off(event.pad_index, note);
    } else if (event.type == musin::ui::DrumpadEvent::Type::Hold) {
      logger.debug("HELD ", static_cast<uint32_t>(event.pad_index));
    }
  }
  logger.debug("Raw_value ", static_cast<uint32_t>(event.raw_value));
}

// --- AnalogControlComponent ---
PizzaControls::AnalogControlComponent::AnalogControlComponent(
    PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      mux_controls{
          AnalogControl{FILTER, true, true},  AnalogControl{PITCH1, true, true},
          AnalogControl{PITCH2, true, true},  AnalogControl{RANDOM, true, true},
          AnalogControl{VOLUME, false, true}, AnalogControl{PITCH3, true, true},
          AnalogControl{SWING, true, true},   AnalogControl{CRUSH, true, true},
          AnalogControl{REPEAT, true, true},  AnalogControl{SPEED, false, true},
          AnalogControl{PITCH4, true, true}},
      control_observers{AnalogControlEventHandler{this, FILTER},
                        AnalogControlEventHandler{this, PITCH1},
                        AnalogControlEventHandler{this, PITCH2},
                        AnalogControlEventHandler{this, RANDOM},
                        AnalogControlEventHandler{this, VOLUME},
                        AnalogControlEventHandler{this, PITCH3},
                        AnalogControlEventHandler{this, SWING},
                        AnalogControlEventHandler{this, CRUSH},
                        AnalogControlEventHandler{this, REPEAT},
                        AnalogControlEventHandler{this, SPEED},
                        AnalogControlEventHandler{this, PITCH4}} {
}

void PizzaControls::AnalogControlComponent::init() {
  // First, initialize controls but don't add observers yet
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].init();
  }

  // Pre-scan all analog inputs to initialize with actual hardware values
  // This prevents the initial flood of MIDI CCs on startup
  parent_controls->_scanner.scan();
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    uint16_t raw_value =
        parent_controls->_scanner.get_raw_value(mux_controls[i].get_id());
    mux_controls[i].update(
        raw_value); // This will set the initial _last_notified_value
  }

  // Now add observers after controls are initialized with real values
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].add_observer(control_observers[i]);
  }
}

void PizzaControls::AnalogControlComponent::update(absolute_time_t now) {
  if (!mux_controls.empty()) {
    auto &control = mux_controls[_next_analog_control_to_update_idx];
    uint16_t raw_value =
        parent_controls->_scanner.get_raw_value(control.get_id());
    control.update(raw_value);
    _next_analog_control_to_update_idx =
        (_next_analog_control_to_update_idx + 1) % mux_controls.size();
  }

  if (is_nil_time(last_smoothing_time_))
    last_smoothing_time_ = now;
  int64_t dt_us = absolute_time_diff_us(last_smoothing_time_, now);
  if (dt_us > 0) {
    float dt_s = static_cast<float>(dt_us) / 1000000.0f;
    last_smoothing_time_ = now;
    if (std::fabs(filter_current_value_ - filter_target_value_) > 0.001f) {
      float alpha =
          1.0f -
          std::exp(-config::analog_controls::FILTER_SMOOTHING_RATE * dt_s);
      filter_current_value_ =
          std::lerp(filter_current_value_, filter_target_value_, alpha);
      parent_controls->_message_router_ref.set_parameter(
          drum::Parameter::FILTER_FREQUENCY, filter_current_value_);
      parent_controls->_message_router_ref.set_parameter(
          drum::Parameter::FILTER_RESONANCE, (1.0f - filter_current_value_));
    }
  }
}

void PizzaControls::AnalogControlComponent::AnalogControlEventHandler::
    notification(musin::ui::AnalogControlEvent event) {
  PizzaControls *controls = parent->parent_controls;
  switch (event.control_id) {
  case FILTER:
    parent->filter_target_value_ = event.value;
    break;
  case RANDOM: {
    bool was_active = controls->_sequencer_controller_ref.is_random_active();
    bool should_be_active =
        (event.value >= config::analog_controls::RANDOM_ACTIVATION_THRESHOLD);
    if (should_be_active && !was_active)
      controls->_sequencer_controller_ref.activate_random();
    else if (!should_be_active && was_active)
      controls->_sequencer_controller_ref.deactivate_random();
    controls->_sequencer_controller_ref.set_random(event.value);
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::RANDOM_EFFECT, event.value, 0);
  } break;
  case VOLUME:
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::VOLUME, event.value);
    break;
  case SWING: {
    float distance_from_center =
        fabsf(event.value - config::analog_controls::SWING_KNOB_CENTER_VALUE);
    uint8_t swing_percent =
        config::analog_controls::SWING_BASE_PERCENT +
        static_cast<uint8_t>(
            distance_from_center *
            config::analog_controls::SWING_PERCENT_SENSITIVITY);
    bool delay_odd =
        (event.value > config::analog_controls::SWING_KNOB_CENTER_VALUE);
    controls->_sequencer_controller_ref.set_swing_target(delay_odd);
    controls->_sequencer_controller_ref.set_swing_percent(swing_percent);
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::SWING, event.value, 0);
  } break;
  case CRUSH:
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::CRUSH_EFFECT, event.value);
    break;
  case REPEAT: {
    std::optional<uint32_t> intended_length = std::nullopt;
    if (event.value >= config::analog_controls::REPEAT_MODE_2_THRESHOLD)
      intended_length = config::analog_controls::REPEAT_LENGTH_MODE_2;
    else if (event.value >= config::analog_controls::REPEAT_MODE_1_THRESHOLD)
      intended_length = config::analog_controls::REPEAT_LENGTH_MODE_1;
    controls->_sequencer_controller_ref.set_intended_repeat_state(
        intended_length);
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::REPEAT_EFFECT, event.value);
  } break;
  case PITCH1:
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::PITCH, event.value, 0);
    break;
  case PITCH2:
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::PITCH, event.value, 1);
    break;
  case PITCH3:
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::PITCH, event.value, 2);
    break;
  case PITCH4:
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::PITCH, event.value, 3);
    break;
  case SPEED: {
    if (controls->_tempo_handler_ref.get_clock_source() ==
        musin::timing::ClockSource::INTERNAL) {
      // Internal clock: existing BPM behavior
      float bpm = config::analog_controls::MIN_BPM_ADJUST +
                  event.value * (config::analog_controls::MAX_BPM_ADJUST -
                                 config::analog_controls::MIN_BPM_ADJUST);
      controls->_tempo_handler_ref.set_bpm(bpm);
    } else {
      // External clock: pot controls speed modifier
      musin::timing::SpeedModifier modifier =
          musin::timing::SpeedModifier::NORMAL_SPEED;
      if (event.value < 0.1f) {
        modifier = musin::timing::SpeedModifier::HALF_SPEED;
      } else if (event.value > 0.9f) {
        modifier = musin::timing::SpeedModifier::DOUBLE_SPEED;
      }
      controls->_tempo_handler_ref.set_speed_modifier(modifier);
    }
    parent->parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::TEMPO, event.value);
  } break;
  }
}

// --- PlaybuttonComponent ---
PizzaControls::PlaybuttonComponent::PlaybuttonComponent(
    PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      playbutton{PLAYBUTTON, config::drumpad::play_button_config},
      playbutton_observer(this, parent_ptr->_logger_ref) {
}

void PizzaControls::PlaybuttonComponent::init() {
  playbutton.init();
  playbutton.add_observer(playbutton_observer);
}

void PizzaControls::PlaybuttonComponent::update() {
  uint16_t raw_value =
      parent_controls->_scanner.get_raw_value(playbutton.get_id());
  playbutton.update(raw_value);
}

void PizzaControls::PlaybuttonComponent::PlaybuttonEventHandler::notification(
    musin::ui::DrumpadEvent event) {
  logger.debug("Playbutton event for pad: ",
               static_cast<uint32_t>(event.pad_index));

  if (event.velocity.has_value()) {
    logger.debug("Velocity ", static_cast<uint32_t>(event.velocity.value()));
  }

  if (event.type == musin::ui::DrumpadEvent::Type::Press) {
    logger.debug("PLAYBUTTON PRESSED");
    parent->parent_controls->_sequencer_controller_ref.toggle();
  } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
    logger.debug("PLAYBUTTON RELEASED");
  } else if (event.type == musin::ui::DrumpadEvent::Type::Hold) {
    logger.debug("PLAYBUTTON HELD - entering sleep mode");
    parent->parent_controls->_system_state_machine_ref.transition_to(
        drum::SystemStateId::Sleep);
  }
  logger.debug("Raw value ", static_cast<uint32_t>(event.raw_value));
}

} // namespace drum
