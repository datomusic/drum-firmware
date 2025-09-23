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

// Pressure-sensitive button configurations
namespace {
constexpr musin::ui::PressureSensitiveButtonConfig RANDOM_BUTTON_CONFIG = {
    .light_press_threshold = 0.2f,
    .hard_press_threshold = 0.7f,
    .light_release_threshold = 0.15f,
    .hard_release_threshold = 0.65f,
    .debounce_ms = 30};

constexpr musin::ui::PressureSensitiveButtonConfig REPEAT_BUTTON_CONFIG = {
    .light_press_threshold =
        drum::config::analog_controls::REPEAT_MODE_1_THRESHOLD,
    .hard_press_threshold =
        drum::config::analog_controls::REPEAT_MODE_2_THRESHOLD,
    .light_release_threshold =
        drum::config::analog_controls::REPEAT_MODE1_EXIT_THRESHOLD,
    .hard_release_threshold =
        drum::config::analog_controls::REPEAT_MODE2_EXIT_THRESHOLD,
    .debounce_ms = drum::config::analog_controls::REPEAT_RUNNING_DEBOUNCE_MS};
} // namespace

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

  // Track initial running state for edge detection
  _was_running_ = is_running();
}

void PizzaControls::update(absolute_time_t now) {
  if (_message_router_ref.get_local_control_mode() ==
      drum::LocalControlMode::ON) {
    // Detect transition from running -> stopped and clear repeat state
    bool running_now = is_running();
    if (_was_running_ && !running_now) {
      analog_component.reset_repeat_state();
    }
    _was_running_ = running_now;

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

void PizzaControls::notification(
    [[maybe_unused]] musin::timing::TempoEvent event) {
  // Only process if sequencer is running
  if (!is_running()) {
    // If sequencer stops, deactivate all cycling
    for (auto &state : keypad_component.cycling_states_) {
      state.next_active = false;
      state.prev_active = false;
    }
    return;
  }

  // Only cycle when the sequencer step actually advances
  uint32_t current_step = _sequencer_controller_ref.get_current_step();

  // Check each pad for active cycling
  for (size_t pad_index = 0;
       pad_index < keypad_component.cycling_states_.size(); ++pad_index) {
    auto &pad_state = keypad_component.cycling_states_[pad_index];

    if (pad_state.is_cycling() && current_step != pad_state.last_step) {
      pad_state.last_step = current_step;

      // Advance the sample for this pad
      int8_t direction = pad_state.get_direction();
      drumpad_component.select_note_for_pad(static_cast<uint8_t>(pad_index),
                                            direction);
    }
  }
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

  // Determine pad_index and offset from event.row
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

  if (event.type == musin::ui::KeypadEvent::Type::Press) {
    // Existing press logic
    controls->drumpad_component.select_note_for_pad(pad_index, offset);
    if (!controls->is_running()) {
      uint8_t note_to_play =
          controls->drumpad_component.get_note_for_pad(pad_index);
      controls->_sequencer_controller_ref.trigger_note_on(
          pad_index, note_to_play, config::keypad::PREVIEW_NOTE_VELOCITY);
    }
  } else if (event.type == musin::ui::KeypadEvent::Type::Hold) {
    // Start cycling for this pad/direction
    auto &pad_state = parent->cycling_states_[pad_index];
    if (offset > 0) {
      pad_state.next_active = true;
    } else {
      pad_state.prev_active = true;
    }
  } else if (event.type == musin::ui::KeypadEvent::Type::Release) {
    // Stop cycling for this pad/direction
    auto &pad_state = parent->cycling_states_[pad_index];
    if (offset > 0) {
      pad_state.next_active = false;
    } else {
      pad_state.prev_active = false;
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
      // Mark sequencer state dirty after pattern change
      controls->_sequencer_controller_ref.mark_state_dirty_public();
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
    // Mark sequencer state dirty after velocity edit
    controls->_sequencer_controller_ref.mark_state_dirty_public();
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
            static_cast<uint8_t>(i), drum::RetriggerMode::Step);
        controls->_sequencer_controller_ref.set_pad_pressed_state(
            static_cast<uint8_t>(i), true);
      } else if (current_mode == musin::ui::RetriggerMode::Double) {
        controls->_sequencer_controller_ref.activate_play_on_every_step(
            static_cast<uint8_t>(i), drum::RetriggerMode::Substeps);
        controls->_sequencer_controller_ref.set_pad_pressed_state(
            static_cast<uint8_t>(i), true);
      } else {
        controls->_sequencer_controller_ref.deactivate_play_on_every_step(
            static_cast<uint8_t>(i));
        controls->_sequencer_controller_ref.set_pad_pressed_state(
            static_cast<uint8_t>(i), false);
      }
    }
    _last_known_retrigger_mode_per_pad[i] = current_mode;
  }
}

void PizzaControls::DrumpadComponent::select_note_for_pad(uint8_t pad_index,
                                                          int8_t offset) {
  if (pad_index >= config::NUM_TRACKS)
    return;

  const auto &track_range = config::track_ranges[pad_index];
  uint8_t current_note =
      parent_controls->_sequencer_controller_ref.get_active_note_for_track(
          pad_index);

  // Calculate new note with wrapping within track range
  int16_t new_note = current_note + offset;

  // Wrap around within the track's note range
  while (new_note < track_range.low_note) {
    new_note += (track_range.high_note - track_range.low_note + 1);
  }
  while (new_note > track_range.high_note) {
    new_note -= (track_range.high_note - track_range.low_note + 1);
  }

  uint8_t new_selected_note_value = static_cast<uint8_t>(new_note);

  parent_controls->_sequencer_controller_ref.set_active_note_for_track(
      pad_index, new_selected_note_value);
  parent_controls->_sequencer_controller_ref.get_sequencer()
      .get_track(pad_index)
      .set_note(new_selected_note_value);
  // Mark sequencer state dirty after changing track note assignments
  parent_controls->_sequencer_controller_ref.mark_state_dirty_public();
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
      if (event.velocity.has_value()) {
        uint8_t note = parent->get_note_for_pad(event.pad_index);
        uint8_t velocity = event.velocity.value();
        seq_controller.trigger_note_on(event.pad_index, note, velocity);
        seq_controller.record_velocity_hit(event.pad_index);
      }
    } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
      logger.debug("RELEASED ", static_cast<uint32_t>(event.pad_index));
      uint8_t note = parent->get_note_for_pad(event.pad_index);
      seq_controller.trigger_note_off(event.pad_index, note);
      seq_controller.clear_velocity_hit(event.pad_index);
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
                        AnalogControlEventHandler{this, PITCH4}},
      random_button_(RANDOM, RANDOM_BUTTON_CONFIG),
      repeat_button_(REPEAT, REPEAT_BUTTON_CONFIG),
      random_button_observer_(this, RANDOM),
      repeat_button_observer_(this, REPEAT) {
}

void PizzaControls::AnalogControlComponent::init() {
  parent_controls->_logger_ref.info(
      "AnalogControlComponent: Starting analog priming...");

  // Prime the analog controls by reading them multiple times.
  // This allows the ADC to stabilize and the internal filters to converge.
  const int PRIMING_LOOPS = 15;
  for (int i = 0; i < PRIMING_LOOPS; ++i) {
    parent_controls->_scanner.scan();
    sleep_ms(5); // Increased delay between priming scans
  }

  parent_controls->_logger_ref.info(
      "AnalogControlComponent: Priming complete. Initializing controls.");

  // Perform the final, definitive scan.
  parent_controls->_scanner.scan();

  for (size_t i = 0; i < mux_controls.size(); ++i) {
    uint16_t raw_value =
        parent_controls->_scanner.get_raw_value(mux_controls[i].get_id());
    mux_controls[i].init(raw_value);

    // Propagate the initial state of the control to the rest of the system.
    handle_control_change(mux_controls[i].get_id(),
                          mux_controls[i].get_value());
  }

  // Add observers now that controls are stably initialized.
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].add_observer(control_observers[i]);
  }

  // Initialize pressure-sensitive buttons
  random_button_.add_observer(random_button_observer_);
  repeat_button_.add_observer(repeat_button_observer_);

  parent_controls->_logger_ref.info(
      "AnalogControlComponent: Initialization complete");
}

void PizzaControls::AnalogControlComponent::update(absolute_time_t now) {
  if (!mux_controls.empty()) {
    auto &control = mux_controls[_next_analog_control_to_update_idx];
    uint16_t raw_value =
        parent_controls->_scanner.get_raw_value(control.get_id());
    control.update(raw_value);

    // Update pressure-sensitive buttons for RANDOM and REPEAT
    if (control.get_id() == RANDOM) {
      random_button_.update(control.get_value(), now);
    } else if (control.get_id() == REPEAT) {
      repeat_button_.update(control.get_value(), now);
    }

    _next_analog_control_to_update_idx =
        (_next_analog_control_to_update_idx + 1) % mux_controls.size();
  }

  if (is_nil_time(last_smoothing_time_))
    last_smoothing_time_ = now;
  int64_t dt_us = absolute_time_diff_us(last_smoothing_time_, now);
  if (dt_us > 0) {
    float dt_s = static_cast<float>(dt_us) / 1000000.0f;
    last_smoothing_time_ = now;

    // The smoothing logic is now always active, but since init ensures
    // current == target, it won't ramp until the user moves the knob.
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

void PizzaControls::AnalogControlComponent::reset_repeat_state() {
  // Clear repeat button state when transitioning from running to stopped
  repeat_stopped_mode_active_ = false;
  // Also ensure the engine isn't left with an active repeat intention
  parent_controls->_sequencer_controller_ref.set_intended_repeat_state(
      std::nullopt);
}

void PizzaControls::AnalogControlComponent::handle_control_change(
    uint16_t control_id, float value) {
  PizzaControls *controls = parent_controls;

  switch (control_id) {
  case FILTER:
    // For initialization, set current value directly.
    // For subsequent events, this just updates the target for smoothing.
    if (is_nil_time(last_smoothing_time_)) { // A proxy for initialization
      filter_current_value_ = value;
    }
    filter_target_value_ = value;
    // The actual set_parameter for FILTER is handled in the update() smoothing
    // loop to create a smooth transition.
    // However, we must send the initial value directly.
    parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::FILTER_FREQUENCY, value);
    parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::FILTER_RESONANCE, (1.0f - value));
    break;
  case RANDOM:
    controls->_sequencer_controller_ref.set_random(value);
    parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::RANDOM_EFFECT, value, 0);
    break;
  case VOLUME:
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::VOLUME,
                                                       value);
    break;
  case SWING: {
    // Swing is ON/OFF with deterministic sign: right of center delays odd steps
    float distance_from_center =
        fabsf(value - config::analog_controls::SWING_KNOB_CENTER_VALUE);
    bool swing_on = (distance_from_center >=
                     config::analog_controls::SWING_ON_OFF_DEADBAND);
    bool delay_odd = (value > config::analog_controls::SWING_KNOB_CENTER_VALUE);
    // Remember the sign regardless of ON/OFF, so toggling later is stable
    controls->_sequencer_controller_ref.set_swing_target(delay_odd);
    controls->_sequencer_controller_ref.set_swing_enabled(swing_on);
    // Forward raw value for UI feedback/telemetry if needed.
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::SWING,
                                                       value, 0);
  } break;
  case CRUSH:
    parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::CRUSH_EFFECT, value);
    break;
  case REPEAT:
    parent_controls->_message_router_ref.set_parameter(
        drum::Parameter::REPEAT_EFFECT, value);
    break;
  case PITCH1:
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::PITCH,
                                                       value, 0);
    break;
  case PITCH2:
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::PITCH,
                                                       value, 1);
    break;
  case PITCH3:
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::PITCH,
                                                       value, 2);
    break;
  case PITCH4:
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::PITCH,
                                                       value, 3);
    break;
  case SPEED: {
    if (controls->_tempo_handler_ref.get_clock_source() ==
        musin::timing::ClockSource::INTERNAL) {
      // Internal clock: existing BPM behavior
      float bpm = config::analog_controls::MIN_BPM_ADJUST +
                  value * (config::analog_controls::MAX_BPM_ADJUST -
                           config::analog_controls::MIN_BPM_ADJUST);
      controls->_tempo_handler_ref.set_bpm(bpm);
    } else {
      // External clock: pot controls speed modifier
      musin::timing::SpeedModifier modifier =
          musin::timing::SpeedModifier::NORMAL_SPEED;
      if (value < 0.1f) {
        modifier = musin::timing::SpeedModifier::HALF_SPEED;
      } else if (value > 0.9f) {
        modifier = musin::timing::SpeedModifier::DOUBLE_SPEED;
      }
      controls->_tempo_handler_ref.set_speed_modifier(modifier);
    }
    parent_controls->_message_router_ref.set_parameter(drum::Parameter::TEMPO,
                                                       value);
  } break;
  }
}

void PizzaControls::AnalogControlComponent::AnalogControlEventHandler::
    notification(musin::ui::AnalogControlEvent event) {
  parent->handle_control_change(event.control_id, event.value);
}

void PizzaControls::AnalogControlComponent::PressureButtonEventHandler::
    notification(musin::ui::PressureSensitiveButtonEvent event) {
  PizzaControls *controls = parent->parent_controls;

  if (event.button_id == RANDOM) {
    if (event.state == musin::ui::PressureState::LightPress &&
        event.previous_state == musin::ui::PressureState::Released) {
      // Light press: existing random behavior
      if (controls->is_running()) {
        controls->_sequencer_controller_ref
            .trigger_random_hard_press_behavior();
      } else {
        controls->_sequencer_controller_ref.trigger_random_steps_when_stopped();
        controls->_sequencer_controller_ref.start_random_step_highlighting();
      }
    } else if (event.state == musin::ui::PressureState::Released) {
      // Button released: stop highlighting random steps
      if (!controls->is_running()) {
        controls->_sequencer_controller_ref.stop_random_step_highlighting();
      }
    } else if (event.state == musin::ui::PressureState::HardPress &&
               event.previous_state == musin::ui::PressureState::LightPress) {
      // Hard press: TODO - implement new behavior
    }
  } else if (event.button_id == REPEAT) {
    if (!controls->is_running()) {
      // When stopped: light press advances step
      if (event.state == musin::ui::PressureState::LightPress &&
          event.previous_state == musin::ui::PressureState::Released) {
        controls->_sequencer_controller_ref.advance_step();
        parent->repeat_stopped_mode_active_ = true;
      } else if (event.state == musin::ui::PressureState::Released) {
        parent->repeat_stopped_mode_active_ = false;
      }
    } else {
      // When running: set repeat mode based on pressure state
      std::optional<uint32_t> intended_length = std::nullopt;
      if (event.state == musin::ui::PressureState::HardPress) {
        intended_length = config::analog_controls::REPEAT_LENGTH_MODE_2;
      } else if (event.state == musin::ui::PressureState::LightPress) {
        intended_length = config::analog_controls::REPEAT_LENGTH_MODE_1;
      }
      controls->_sequencer_controller_ref.set_intended_repeat_state(
          intended_length);
    }
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

    // If we just started, trigger sync behavior for better phase alignment
    if (parent->parent_controls->_sequencer_controller_ref.is_running()) {
      parent->parent_controls->_tempo_handler_ref.trigger_manual_sync();
    }
  } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
    logger.debug("PLAYBUTTON RELEASED");
  } else if (event.type == musin::ui::DrumpadEvent::Type::Hold) {
    logger.debug("PLAYBUTTON HELD - entering sleep mode");
    parent->parent_controls->_system_state_machine_ref.transition_to(
        drum::SystemStateId::FallingAsleep);
  }
  logger.debug("Raw value ", static_cast<uint32_t>(event.raw_value));
}

} // namespace drum
