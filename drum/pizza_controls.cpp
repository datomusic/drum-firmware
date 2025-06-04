#include "pizza_controls.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "pico/time.h"
#include "pizza_display.h"
#include "sequencer_controller.h"
#include "sound_router.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace drum {

using musin::hal::AnalogInMux16;
using musin::ui::AnalogControl;
using musin::ui::Drumpad;

PizzaControls::PizzaControls(drum::PizzaDisplay &display_ref,
                             musin::timing::TempoHandler &tempo_handler_ref,
                             drum::DefaultSequencerController &sequencer_controller_ref,
                             drum::SoundRouter &sound_router_ref)
    : display(display_ref), _tempo_handler_ref(tempo_handler_ref),
      _sequencer_controller_ref(sequencer_controller_ref), _sound_router_ref(sound_router_ref),
      keypad_component(this), drumpad_component(this), analog_component(this),
      _profiler(config::PROFILER_REPORT_INTERVAL_MS), playbutton_component(this) {
}

void PizzaControls::init() {
  keypad_component.init();
  drumpad_component.init();
  analog_component.init();
  playbutton_component.init();

  _profiler.add_section("Keypad Update");
  _profiler.add_section("Drumpad Update");
  _profiler.add_section("Analog Update");
  _profiler.add_section("Playbutton Update");
}

void PizzaControls::update() {
  {
    musin::hal::DebugUtils::ScopedProfile p(_profiler,
                                            static_cast<size_t>(ProfileSection::KEYPAD_UPDATE));
    keypad_component.update();
  }
  {
    musin::hal::DebugUtils::ScopedProfile p(_profiler,
                                            static_cast<size_t>(ProfileSection::DRUMPAD_UPDATE));
    drumpad_component.update();
  }
  {
    musin::hal::DebugUtils::ScopedProfile p(_profiler,
                                            static_cast<size_t>(ProfileSection::ANALOG_UPDATE));
    analog_component.update();
  }
  {
    musin::hal::DebugUtils::ScopedProfile p(_profiler,
                                            static_cast<size_t>(ProfileSection::PLAYBUTTON_UPDATE));
    playbutton_component.update(); // Updates the *input* state of the button
  }
  _profiler.check_and_print_report();
}

bool PizzaControls::is_running() const {
  return _sequencer_controller_ref.is_running();
}

PizzaControls::KeypadComponent::KeypadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      keypad(keypad_decoder_pins, keypad_columns_pins, config::keypad::DEBOUNCE_TIME_MS,
             config::keypad::POLL_INTERVAL_MS, config::keypad::HOLD_TIME_MS),
      keypad_observer(this, keypad_cc_map, config::keypad::_CHANNEL) {
}

void PizzaControls::KeypadComponent::init() {
  keypad.init();
  keypad.add_observer(keypad_observer);
}

void PizzaControls::KeypadComponent::update() {
  keypad.scan();
}

void PizzaControls::KeypadComponent::KeypadEventHandler::notification(
    musin::ui::KeypadEvent event) {
  PizzaControls *controls = parent->parent_controls;

  // Sample Select (Column 4)
  if (event.col >= config::keypad::SAMPLE_SELECT_START_COLUMN) {
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
        uint8_t note_to_play = controls->drumpad_component.get_note_for_pad(pad_index);
        drum::Events::NoteEvent note_event{.track_index = pad_index,
                                           .note = note_to_play,
                                           .velocity = config::keypad::PREVIEW_NOTE_VELOCITY};
        controls->_sequencer_controller_ref.trigger_note_on(pad_index, note_to_play,
                                                            config::keypad::PREVIEW_NOTE_VELOCITY);
      }
    }
    return;
  }

  // Map physical column to logical track (0->3, 1->2, 2->1, 3->0)
  uint8_t track_idx = (drum::PizzaDisplay::SEQUENCER_TRACKS_DISPLAYED - 1) - event.col;
  uint8_t step_idx = (KEYPAD_ROWS - 1) - event.row; // Map row to step index (0-7)

  // Get a reference to the track to modify it
  auto &track = controls->_sequencer_controller_ref.get_sequencer().get_track(track_idx);

  uint8_t step_velocity;
  bool now_enabled;

  if (event.type == musin::ui::KeypadEvent::Type::Press) {
    now_enabled = track.toggle_step_enabled(step_idx);
    step_velocity = config::keypad::DEFAULT_STEP_VELOCITY;
  } else if (event.type == musin::ui::KeypadEvent::Type::Hold) {
    step_velocity = config::keypad::MAX_STEP_VELOCITY_ON_HOLD;
  } else if (event.type == musin::ui::KeypadEvent::Type::Tap) {
    now_enabled = track.toggle_step_enabled(step_idx);
    step_velocity = config::keypad::STEP_VELOCITY_ON_TAP;
  }

  if (now_enabled) {
    // Get the current note assigned to the corresponding drumpad
    uint8_t note = controls->drumpad_component.get_note_for_pad(track_idx);
    track.set_step_note(step_idx, note);

    track.set_step_velocity(step_idx, step_velocity);

    if (!controls->is_running()) {
      controls->_sequencer_controller_ref.trigger_note_on(track_idx, note, step_velocity);
    }
  }
}

PizzaControls::DrumpadComponent::DrumpadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      drumpad_readers{AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_1},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_2},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_3},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_4}},
      drumpads{Drumpad<AnalogInMux16>{
                   drumpad_readers[0], 0, config::drumpad::DEBOUNCE_PRESS_MS,
                   config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS,
                   config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS,
                   config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE,
                   config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE},
               Drumpad<AnalogInMux16>{
                   drumpad_readers[1], 1, config::drumpad::DEBOUNCE_PRESS_MS,
                   config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS,
                   config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS,
                   config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE,
                   config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE},
               Drumpad<AnalogInMux16>{
                   drumpad_readers[2], 2, config::drumpad::DEBOUNCE_PRESS_MS,
                   config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS,
                   config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS,
                   config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE,
                   config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE},
               Drumpad<AnalogInMux16>{
                   drumpad_readers[3], 3, config::drumpad::DEBOUNCE_PRESS_MS,
                   config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS,
                   config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS,
                   config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE,
                   config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE}},
      drumpad_observers{DrumpadEventHandler{this, 0}, DrumpadEventHandler{this, 1},
                        DrumpadEventHandler{this, 2}, DrumpadEventHandler{this, 3}} {
}

void PizzaControls::DrumpadComponent::init() {
  for (auto &reader : drumpad_readers) {
    reader.init();
  }
  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].add_observer(drumpad_observers[i]);
  }
}

void PizzaControls::DrumpadComponent::update() {
  update_drumpads();
}

void PizzaControls::DrumpadComponent::update_drumpads() {
  PizzaControls *controls = parent_controls;

  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].update();

    // Update SequencerController with any changes to retrigger mode
    musin::ui::RetriggerMode current_mode = drumpads[i].get_retrigger_mode();
    if (current_mode != _last_known_retrigger_mode_per_pad[i]) {
      if (current_mode == musin::ui::RetriggerMode::Single) {
        controls->_sequencer_controller_ref.activate_play_on_every_step(static_cast<uint8_t>(i), 1);
      } else if (current_mode == musin::ui::RetriggerMode::Double) {
        controls->_sequencer_controller_ref.activate_play_on_every_step(static_cast<uint8_t>(i), 2);
      } else { // Off or any other state
        controls->_sequencer_controller_ref.deactivate_play_on_every_step(static_cast<uint8_t>(i));
      }
    }
    _last_known_retrigger_mode_per_pad[i] = current_mode;

    // The drumpad LED colors, including fades, are handled by PizzaDisplay::draw_animations(),
    // which is called in the main loop. PizzaControls updates the necessary state
    // (e.g., active note via SequencerController, fade start times via NoteEvents)
    // that PizzaDisplay uses for drawing.
  }
}

void PizzaControls::DrumpadComponent::select_note_for_pad(uint8_t pad_index, int8_t offset) {
  if (pad_index >= config::track_note_ranges.size()) {
    return;
  }

  const auto &notes_for_pad = config::track_note_ranges[pad_index];
  if (notes_for_pad.empty()) {
    return;
  }

  uint8_t current_note =
      parent_controls->_sequencer_controller_ref.get_active_note_for_track(pad_index);
  size_t num_notes_in_list = notes_for_pad.size();
  int32_t current_list_idx = -1;

  // Find the index of the current_note in the notes_for_pad list
  for (size_t i = 0; i < num_notes_in_list; ++i) {
    if (notes_for_pad[i] == current_note) {
      current_list_idx = static_cast<int32_t>(i);
      break;
    }
  }

  // If current_note wasn't found (e.g. initial state or inconsistency), default to index 0
  if (current_list_idx == -1) {
    current_list_idx = 0;
  }

  int32_t new_list_idx = (current_list_idx + offset);
  // Modulo arithmetic to wrap around the list
  new_list_idx = (new_list_idx % static_cast<int32_t>(num_notes_in_list) +
                  static_cast<int32_t>(num_notes_in_list)) %
                 static_cast<int32_t>(num_notes_in_list);

  uint8_t new_selected_note_value = notes_for_pad[static_cast<size_t>(new_list_idx)];

  // Update the active note in SequencerController
  parent_controls->_sequencer_controller_ref.set_active_note_for_track(pad_index,
                                                                       new_selected_note_value);

  // Update the default note for new steps in the sequencer track
  parent_controls->_sequencer_controller_ref.get_sequencer().get_track(pad_index).set_note(
      new_selected_note_value);
}

uint8_t PizzaControls::DrumpadComponent::get_note_for_pad(uint8_t pad_index) const {
  if (pad_index >= config::NUM_DRUMPADS) {
    return config::drumpad::DEFAULT_FALLBACK_NOTE;
  }
  return parent_controls->_sequencer_controller_ref.get_active_note_for_track(pad_index);
}

void PizzaControls::DrumpadComponent::DrumpadEventHandler::notification(
    musin::ui::DrumpadEvent event) {
  // parent_controls is PizzaControls, which has _sequencer_controller_ref
  auto &seq_controller = parent->parent_controls->_sequencer_controller_ref;

  if (event.pad_index < config::NUM_DRUMPADS) { // Check against known drumpad count
    if (event.type == musin::ui::DrumpadEvent::Type::Press) {
      seq_controller.set_pad_pressed_state(event.pad_index, true);
      if (event.velocity.has_value()) {
        uint8_t note = parent->get_note_for_pad(event.pad_index);
        uint8_t velocity = event.velocity.value();
        seq_controller.trigger_note_on(event.pad_index, note, velocity);
      }
    } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
      seq_controller.set_pad_pressed_state(event.pad_index, false);
      uint8_t note = parent->get_note_for_pad(event.pad_index);
      seq_controller.trigger_note_off(event.pad_index, note);
    }
  }
}

// --- AnalogControlComponent Implementation ---

PizzaControls::AnalogControlComponent::AnalogControlComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      mux_controls{AnalogControl{PIN_ADC, analog_address_pins, FILTER, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH1, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH2, true},
                   AnalogControl{PIN_ADC, analog_address_pins, RANDOM, true},
                   AnalogControl{PIN_ADC, analog_address_pins, VOLUME},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH3, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SWING, true},
                   AnalogControl{PIN_ADC, analog_address_pins, CRUSH, true},
                   AnalogControl{PIN_ADC, analog_address_pins, REPEAT, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SPEED, false},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH4, true}},
      control_observers{
          AnalogControlEventHandler{this, FILTER}, AnalogControlEventHandler{this, PITCH1},
          AnalogControlEventHandler{this, PITCH2}, AnalogControlEventHandler{this, RANDOM},
          AnalogControlEventHandler{this, VOLUME}, AnalogControlEventHandler{this, PITCH3},
          AnalogControlEventHandler{this, SWING},  AnalogControlEventHandler{this, CRUSH},
          AnalogControlEventHandler{this, REPEAT}, AnalogControlEventHandler{this, SPEED},
          AnalogControlEventHandler{this, PITCH4}} {
}

void PizzaControls::AnalogControlComponent::init() {
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].init();
    mux_controls[i].add_observer(control_observers[i]);
  }
}

void PizzaControls::AnalogControlComponent::update() {
  if (!mux_controls.empty()) {
    mux_controls[_next_analog_control_to_update_idx].update();
    _next_analog_control_to_update_idx =
        (_next_analog_control_to_update_idx + 1) % mux_controls.size();
  }
}

void PizzaControls::AnalogControlComponent::AnalogControlEventHandler::notification(
    musin::ui::AnalogControlEvent event) {
  PizzaControls *controls = parent->parent_controls;
  const uint8_t mux_channel = event.control_id >> 8;

  // Note: RANDOM, SWING, REPEAT, SPEED are handled differently (affect sequencer/clock directly)
  //       and only go through the SoundRouter's parameter setting to send CC's.

  switch (mux_channel) {
  case FILTER:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::FILTER_FREQUENCY,
                                                             event.value);
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::FILTER_RESONANCE,
                                                             (1.0f - event.value));
    break;
  case RANDOM: {
    bool was_active = controls->_sequencer_controller_ref.is_random_active();
    bool should_be_active = (event.value >= config::analog_controls::RANDOM_ACTIVATION_THRESHOLD);

    if (should_be_active && !was_active) {
      controls->_sequencer_controller_ref.activate_random();
    } else if (!should_be_active && was_active) {
      controls->_sequencer_controller_ref.deactivate_random();
    }
    controls->_sequencer_controller_ref.set_random_probability(event.value * 33);
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::RANDOM_EFFECT, event.value, 0);
  } break;
  case VOLUME:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::VOLUME, event.value);
    break;
  case SWING: {
    float distance_from_center =
        fabsf(event.value - config::analog_controls::SWING_KNOB_CENTER_VALUE); // Range 0.0 to 0.5

    uint8_t swing_percent =
        config::analog_controls::SWING_BASE_PERCENT +
        static_cast<uint8_t>(distance_from_center *
                             config::analog_controls::SWING_PERCENT_SENSITIVITY);

    bool delay_odd = (event.value > config::analog_controls::SWING_KNOB_CENTER_VALUE);
    controls->_sequencer_controller_ref.set_swing_target(delay_odd);

    controls->_sequencer_controller_ref.set_swing_percent(swing_percent);
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::SWING, event.value, 0);
    break;
  }
  case CRUSH:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::CRUSH_EFFECT,
                                                             event.value);
    break;
  case REPEAT: {
    // Determine the intended state based on the knob value
    std::optional<uint32_t> intended_length = std::nullopt;
    if (event.value >= config::analog_controls::REPEAT_MODE_2_THRESHOLD) {
      intended_length = config::analog_controls::REPEAT_LENGTH_MODE_2;
    } else if (event.value >= config::analog_controls::REPEAT_MODE_1_THRESHOLD) {
      intended_length = config::analog_controls::REPEAT_LENGTH_MODE_1;
    }

    // Pass the intended state to the sequencer controller
    controls->_sequencer_controller_ref.set_intended_repeat_state(intended_length);
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::REPEAT_EFFECT, event.value);
    break;
  }
  case PITCH1:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value,
                                                             0);
    break;
  case PITCH2:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value,
                                                             1);
    break;
  case PITCH3:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value,
                                                             2);
    break;
  case PITCH4:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value,
                                                             3);
    break;
  case SPEED: {
    float bpm = config::analog_controls::MIN_BPM_ADJUST +
                event.value * (config::analog_controls::MAX_BPM_ADJUST -
                               config::analog_controls::MIN_BPM_ADJUST);
    controls->_tempo_handler_ref.set_bpm(bpm);
    break;
  }
  }
}

PizzaControls::PlaybuttonComponent::PlaybuttonComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      playbutton_reader{AnalogInMux16{PIN_ADC, analog_address_pins, PLAYBUTTON}},
      playbutton{playbutton_reader,
                 0, // id for the Drumpad instance
                 config::drumpad::DEBOUNCE_PRESS_MS,
                 config::drumpad::DEBOUNCE_RELEASE_MS,
                 config::drumpad::HOLD_THRESHOLD_MS,
                 config::drumpad::HOLD_REPEAT_DELAY_MS,
                 config::drumpad::HOLD_REPEAT_INTERVAL_MS,
                 config::drumpad::MIN_PRESSURE_VALUE,
                 config::drumpad::MAX_PRESSURE_VALUE,
                 config::drumpad::MIN_VELOCITY_VALUE,
                 config::drumpad::MAX_VELOCITY_VALUE},
      playbutton_observer(this) {
}

void PizzaControls::PlaybuttonComponent::init() {
  playbutton_reader.init();
  playbutton.add_observer(playbutton_observer);
}

void PizzaControls::PlaybuttonComponent::update() {
  playbutton.update();
}

void PizzaControls::PlaybuttonComponent::PlaybuttonEventHandler::notification(
    musin::ui::DrumpadEvent event) {
  if (event.type == musin::ui::DrumpadEvent::Type::Press) {
    parent->parent_controls->_sequencer_controller_ref.toggle();
  }
}
} // namespace drum
