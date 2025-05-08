#include "pizza_controls.h"
// #include "midi_functions.h" // No longer directly needed here for notes/cc
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "pico/time.h" // For get_absolute_time, to_us_since_boot
#include "pizza_display.h"
#include "sequencer_controller.h"
#include "sound_router.h" // Added
#include <algorithm>      // For std::clamp
#include <cmath>          // For fmodf
#include <cstddef>
#include <cstdio>

namespace drum {

using musin::hal::AnalogInMux16;
using musin::ui::AnalogControl;
using musin::ui::Drumpad;

PizzaControls::PizzaControls(drum::PizzaDisplay &display_ref,
                             musin::timing::Sequencer<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &sequencer_ref,
                             musin::timing::InternalClock &clock_ref,
                             musin::timing::TempoHandler &tempo_handler_ref,
                             musin::timing::TempoMultiplier &tempo_multiplier_ref, // Added
                             drum::DefaultSequencerController &sequencer_controller_ref,
                             drum::SoundRouter &sound_router_ref)
    : display(display_ref), sequencer(sequencer_ref), _internal_clock(clock_ref),
      _tempo_handler_ref(tempo_handler_ref),
      _tempo_multiplier_ref(tempo_multiplier_ref), // Initialize
      _sequencer_controller_ref(sequencer_controller_ref), _sound_router_ref(sound_router_ref),
      keypad_component(this), drumpad_component(this), analog_component(this),
      _profiler(config::PROFILER_REPORT_INTERVAL_MS), playbutton_component(this) {
}

void PizzaControls::init() {
  keypad_component.init();
  drumpad_component.init();
  analog_component.init();
  playbutton_component.init();

  // Register this class to receive tempo events for LED pulsing
  _tempo_handler_ref.add_observer(*this);
  // Register to receive SequencerTickEvents to reset sub-step counter
  _tempo_multiplier_ref.add_observer(*this);

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

  // Update track overrides based on drumpad presses
  for (uint8_t i = 0; i < drumpad_component.get_num_drumpads(); ++i) {
    if (drumpad_component.is_pad_pressed(i)) {
      uint8_t note = drumpad_component.get_note_for_pad(i);
      uint32_t color = display.get_note_color(note % PizzaDisplay::NUM_NOTE_COLORS);
      display.set_track_override_color(i, color);
    } else {
      display.clear_track_override_color(i);
    }
  }

  // Update the play button LED based on sequencer state
  if (_sequencer_controller_ref.is_running()) {
    display.set_play_button_led(drum::PizzaDisplay::COLOR_WHITE);
  } else {
    constexpr uint32_t ticks_per_beat = musin::timing::InternalClock::PPQN;
    uint32_t phase_ticks = 0;
    if (ticks_per_beat > 0) {
      phase_ticks = _clock_tick_counter % ticks_per_beat;
    }
    float brightness_factor = 0.0f;
    if (ticks_per_beat > 0) {
      brightness_factor =
          1.0f - (static_cast<float>(phase_ticks) / static_cast<float>(ticks_per_beat));
    }
    _stopped_highlight_factor = std::clamp(brightness_factor, 0.0f, 1.0f);
    uint8_t brightness = static_cast<uint8_t>(_stopped_highlight_factor * config::DISPLAY_BRIGHTNESS_MAX_VALUE);
    uint32_t base_color = drum::PizzaDisplay::COLOR_WHITE;
    uint32_t pulse_color = display.leds().adjust_color_brightness(base_color, brightness);
    display.set_play_button_led(pulse_color);
  }

  refresh_sequencer_display();

  _profiler.check_and_print_report();
}

void PizzaControls::notification(musin::timing::TempoEvent /* event */) {
  // This notification is now driven by the active clock source via TempoHandler.
  // Only advance the counter if the sequencer is NOT running.
  if (!_sequencer_controller_ref.is_running()) {
    _clock_tick_counter++;
    // The PPQN used for calculation in update() is still valid as it defines
    // the resolution we expect the TempoEvents to represent.
  } else {
    // Sequencer is running, handle retrigger logic
    _clock_tick_counter = 0; // Reset pulse counter

    uint32_t retrigger_period = _sequencer_controller_ref.get_ticks_per_musical_step();

    if (retrigger_period == 0) {
      return;
    }

    for (size_t i = 0; i < drumpad_component.get_num_drumpads(); ++i) {
      const auto &pad = drumpad_component.get_drumpad(i);
      musin::ui::RetriggerMode mode = pad.get_retrigger_mode();
      uint8_t note = drumpad_component.get_note_for_pad(static_cast<uint8_t>(i));

      bool trigger_now = false;
      if (mode == musin::ui::RetriggerMode::Single) {
        if (_sub_step_tick_counter == 0) {
          trigger_now = true;
        }
      } else if (mode == musin::ui::RetriggerMode::Double) {
        if (_sub_step_tick_counter == 0 ||
            (retrigger_period >= config::main_controls::RETRIGGER_DIVISOR_FOR_DOUBLE_MODE &&
             _sub_step_tick_counter == (retrigger_period / config::main_controls::RETRIGGER_DIVISOR_FOR_DOUBLE_MODE))) {
          trigger_now = true;
        }
      }

      if (trigger_now) {
        drum::Events::NoteEvent note_event{
            .track_index = static_cast<uint8_t>(i), .note = note, .velocity = config::drumpad::RETRIGGER_VELOCITY};
        _sound_router_ref.notification(note_event);
      }
    }
    _sub_step_tick_counter++;
    if (_sub_step_tick_counter >= retrigger_period) {
      _sub_step_tick_counter = 0;
    }
  }
}

void PizzaControls::refresh_sequencer_display() {
  bool current_is_running = _sequencer_controller_ref.is_running();
  display.draw_sequencer_state(sequencer, _sequencer_controller_ref, current_is_running,
                               _stopped_highlight_factor);
}

// --- Implementation for getter moved from header ---
bool PizzaControls::is_running() const {
  return _sequencer_controller_ref.is_running();
}
// --- End moved implementation ---

// Notification handler for NoteEvents from SequencerController
void PizzaControls::notification(drum::Events::NoteEvent event) {
  if (event.velocity > 0 && event.track_index < drumpad_component.get_num_drumpads()) {
    drumpad_component.trigger_fade(event.track_index);
  }
}

PizzaControls::KeypadComponent::KeypadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr), keypad(keypad_decoder_pins, keypad_columns_pins, config::keypad::DEBOUNCE_TIME_MS, config::keypad::POLL_INTERVAL_MS, config::keypad::HOLD_TIME_MS),
      keypad_observer(this, keypad_cc_map, config::keypad::MIDI_CHANNEL) {
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
        drum::Events::NoteEvent note_event{
            .track_index = pad_index,
            .note = note_to_play,
            .velocity = config::keypad::PREVIEW_NOTE_VELOCITY
        };
        controls->_sound_router_ref.notification(note_event);
      }
    }
    return;
  }

  // Map physical column to logical track (0->3, 1->2, 2->1, 3->0)
  uint8_t track_idx = (drum::PizzaDisplay::SEQUENCER_TRACKS_DISPLAYED - 1) - event.col;
  uint8_t step_idx = (KEYPAD_ROWS - 1) - event.row; // Map row to step index (0-7)

  // Get a reference to the track to modify it
  auto &track = controls->sequencer.get_track(track_idx);

  if (event.type == musin::ui::KeypadEvent::Type::Press) {
    bool now_enabled = track.toggle_step_enabled(step_idx);

    if (now_enabled) {
      // Get the current note assigned to the corresponding drumpad
      uint8_t note = controls->drumpad_component.get_note_for_pad(track_idx);
      track.set_step_note(step_idx, note);

      uint8_t step_velocity;
      if (!track.get_step_velocity(step_idx).has_value()) {
        track.set_step_velocity(step_idx, config::keypad::DEFAULT_STEP_VELOCITY);
        step_velocity = config::keypad::DEFAULT_STEP_VELOCITY;
      } else {
        step_velocity = track.get_step_velocity(step_idx).value();
      }

      if (!controls->is_running()) {
        drum::Events::NoteEvent note_event{
            .track_index = track_idx, .note = note, .velocity = step_velocity};
        controls->_sound_router_ref.notification(note_event);
      }
    }
  } else if (event.type == musin::ui::KeypadEvent::Type::Hold) {
    track.set_step_velocity(step_idx, config::keypad::MAX_STEP_VELOCITY_ON_HOLD);
  }
}

// --- DrumpadComponent Implementation ---

PizzaControls::DrumpadComponent::DrumpadComponent(PizzaControls *parent_ptr) 
    : parent_controls(parent_ptr), // Removed _sound_router init
      drumpad_readers{AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_1},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_2},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_3},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_4}},
      drumpads{
          Drumpad<AnalogInMux16>{drumpad_readers[0], 0, config::drumpad::DEBOUNCE_PRESS_MS, config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS, config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS, config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE, config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE},
          Drumpad<AnalogInMux16>{drumpad_readers[1], 1, config::drumpad::DEBOUNCE_PRESS_MS, config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS, config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS, config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE, config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE},
          Drumpad<AnalogInMux16>{drumpad_readers[2], 2, config::drumpad::DEBOUNCE_PRESS_MS, config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS, config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS, config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE, config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE},
          Drumpad<AnalogInMux16>{drumpad_readers[3], 3, config::drumpad::DEBOUNCE_PRESS_MS, config::drumpad::DEBOUNCE_RELEASE_MS, config::drumpad::HOLD_THRESHOLD_MS, config::drumpad::HOLD_REPEAT_DELAY_MS, config::drumpad::HOLD_REPEAT_INTERVAL_MS, config::drumpad::MIN_PRESSURE_VALUE, config::drumpad::MAX_PRESSURE_VALUE, config::drumpad::MIN_VELOCITY_VALUE, config::drumpad::MAX_VELOCITY_VALUE}},
      drumpad_note_numbers{}, // Initialize with the first index (0) for each pad's note list
      // _pad_pressed_state is value-initialized by default in the header
      _fade_start_time{}, // Initialize before observers to match declaration order, value-initialized
      drumpad_observers{DrumpadEventHandler{this, 0},
                        DrumpadEventHandler{this, 1},
                        DrumpadEventHandler{this, 2},
                        DrumpadEventHandler{this, 3}} {
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
  absolute_time_t now = get_absolute_time();
  PizzaControls *controls = parent_controls;

  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].update();

    uint8_t current_note_selection_idx = drumpad_note_numbers[i];
    // Ensure the index is valid before accessing, though it should be by design
    uint8_t note_value = (current_note_selection_idx < drumpad_note_ranges[i].size())
                           ? drumpad_note_ranges[i][current_note_selection_idx]
                           : drumpad_note_ranges[i][0]; // Fallback to first note

    auto led_index_opt = controls->display.get_drumpad_led_index(i);

    if (led_index_opt.has_value()) {
      uint32_t led_index = led_index_opt.value();
      uint32_t base_color = controls->display.get_note_color(note_value);
      uint32_t final_color = base_color; // Default to base color

      // Check fade state
      if (!is_nil_time(_fade_start_time[i])) {
        uint64_t time_since_fade_start_us = absolute_time_diff_us(_fade_start_time[i], now);
        uint64_t fade_duration_us = static_cast<uint64_t>(config::drumpad::FADE_DURATION_MS) * 1000;

        if (time_since_fade_start_us < fade_duration_us) {
          // Fade is active: Calculate brightness factor (MIN_FADE_BRIGHTNESS_FACTOR up to 1.0)
          float fade_progress = std::min(1.0f, static_cast<float>(time_since_fade_start_us) /
                                                   static_cast<float>(fade_duration_us));
          float current_brightness_factor =
              config::drumpad::MIN_FADE_BRIGHTNESS_FACTOR + fade_progress * (1.0f - config::drumpad::MIN_FADE_BRIGHTNESS_FACTOR);
          uint8_t brightness_value =
              static_cast<uint8_t>(std::clamp(current_brightness_factor * config::DISPLAY_BRIGHTNESS_MAX_VALUE, 0.0f, config::DISPLAY_BRIGHTNESS_MAX_VALUE));
          final_color =
              controls->display.leds().adjust_color_brightness(base_color, brightness_value);
        } else {
          // Fade finished in this cycle
          _fade_start_time[i] = nil_time; // Reset fade start time
          // Ensure final color is the base color when fade ends
          final_color = base_color;
        }
      }

      controls->display.set_led(led_index, final_color);
    }
  }
}

void PizzaControls::DrumpadComponent::select_note_for_pad(uint8_t pad_index, int8_t offset) {
  if (pad_index >= drumpad_note_ranges.size()) {
    return;
  }

  const auto &notes_for_pad = drumpad_note_ranges[pad_index];
  if (notes_for_pad.empty()) {
    return;
  }

  size_t num_notes_in_list = notes_for_pad.size();
  int32_t current_list_idx = static_cast<int32_t>(drumpad_note_numbers[pad_index]);

  int32_t new_list_idx = (current_list_idx + offset);
  new_list_idx = (new_list_idx % static_cast<int32_t>(num_notes_in_list) +
                  static_cast<int32_t>(num_notes_in_list)) %
                 static_cast<int32_t>(num_notes_in_list);

  drumpad_note_numbers[pad_index] = static_cast<uint8_t>(new_list_idx);

  uint8_t selected_note_value = notes_for_pad[static_cast<uint8_t>(new_list_idx)];

  parent_controls->sequencer.get_track(pad_index).set_note(selected_note_value);

  auto led_index_opt = parent_controls->display.get_drumpad_led_index(pad_index);
  if (led_index_opt.has_value()) {
    uint32_t led_index = led_index_opt.value();
    uint32_t base_color = parent_controls->display.get_note_color(selected_note_value);
    parent_controls->display.set_led(led_index, base_color);
  }
  trigger_fade(pad_index);
}

bool PizzaControls::DrumpadComponent::is_pad_pressed(uint8_t pad_index) const {
  if (pad_index < _pad_pressed_state.size()) {
    return _pad_pressed_state[pad_index];
  }
  return false;
}

uint8_t PizzaControls::DrumpadComponent::get_note_for_pad(uint8_t pad_index) const {
  if (pad_index >= drumpad_note_ranges.size()) {
    return config::drumpad::DEFAULT_FALLBACK_NOTE;
  }

  const auto &notes_for_this_pad = drumpad_note_ranges[pad_index];
  if (notes_for_this_pad.empty()) {
    return config::drumpad::DEFAULT_FALLBACK_NOTE;
  }

  uint8_t current_selection_idx = drumpad_note_numbers[pad_index];
  if (current_selection_idx >= notes_for_this_pad.size()) {
    return notes_for_this_pad[0];
  }

  return notes_for_this_pad[current_selection_idx];
}

void PizzaControls::DrumpadComponent::trigger_fade(uint8_t pad_index) {
  if (pad_index < _fade_start_time.size()) {
    _fade_start_time[pad_index] = get_absolute_time();
  }
}

void PizzaControls::DrumpadComponent::DrumpadEventHandler::notification(
    musin::ui::DrumpadEvent event) {
  if (event.pad_index < parent->_pad_pressed_state.size()) {
    if (event.type == musin::ui::DrumpadEvent::Type::Press) {
      parent->_pad_pressed_state[event.pad_index] = true;
      if (event.velocity.has_value()) {
        parent->trigger_fade(event.pad_index);
        uint8_t note = parent->get_note_for_pad(event.pad_index);
        uint8_t velocity = event.velocity.value();
        drum::Events::NoteEvent note_event{
            .track_index = event.pad_index, .note = note, .velocity = velocity};
        parent->parent_controls->_sound_router_ref.notification(note_event);
      }
    } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
      parent->_pad_pressed_state[event.pad_index] = false;
      uint8_t note = parent->get_note_for_pad(event.pad_index);
      drum::Events::NoteEvent note_event{
          .track_index = event.pad_index, .note = note, .velocity = 0};
      parent->parent_controls->_sound_router_ref.notification(note_event);
    }
  }
}

// --- AnalogControlComponent Implementation ---

PizzaControls::AnalogControlComponent::AnalogControlComponent(
    PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      mux_controls{/*AnalogControl{PIN_ADC, analog_address_pins, DRUM1, true},*/
                   AnalogControl{PIN_ADC, analog_address_pins, FILTER, true},
                   //  AnalogControl{PIN_ADC, analog_address_pins, DRUM2, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH1, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH2, true},
                   //  AnalogControl{PIN_ADC, analog_address_pins, PLAYBUTTON, true},
                   AnalogControl{PIN_ADC, analog_address_pins, RANDOM, true},
                   AnalogControl{PIN_ADC, analog_address_pins, VOLUME},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH3, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SWING, true},
                   AnalogControl{PIN_ADC, analog_address_pins, CRUSH, true},
                   //  AnalogControl{PIN_ADC, analog_address_pins, DRUM3, true},
                   AnalogControl{PIN_ADC, analog_address_pins, REPEAT, true},
                   //  AnalogControl{PIN_ADC, analog_address_pins, DRUM4, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SPEED, false},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH4, true}},
      control_observers{
          // AnalogControlEventHandler{this, DRUM1},
          AnalogControlEventHandler{this, FILTER},
          // AnalogControlEventHandler{this, DRUM2},
          AnalogControlEventHandler{this, PITCH1},
          AnalogControlEventHandler{this, PITCH2},
          // AnalogControlEventHandler{this, PLAYBUTTON},
          AnalogControlEventHandler{this, RANDOM},
          AnalogControlEventHandler{this, VOLUME},
          AnalogControlEventHandler{this, PITCH3},
          AnalogControlEventHandler{this, SWING},
          AnalogControlEventHandler{this, CRUSH},
          // AnalogControlEventHandler{this, DRUM3},
          AnalogControlEventHandler{this, REPEAT},
          // AnalogControlEventHandler{this, DRUM4},
          AnalogControlEventHandler{this, SPEED},
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
  //       and do not go through the SoundRouter's parameter setting.

  switch (mux_channel) {
  case FILTER:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::FILTER_FREQUENCY, event.value);
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::FILTER_RESONANCE, (1.0f - event.value));
    break;
  case RANDOM: {
    bool was_active = controls->_sequencer_controller_ref.is_random_active();
    bool should_be_active = (event.value >= config::analog_controls::RANDOM_ACTIVATION_THRESHOLD);

    if (should_be_active && !was_active) {
      controls->_sequencer_controller_ref.activate_random();
    } else if (!should_be_active && was_active) {
      controls->_sequencer_controller_ref.deactivate_random();
    }
  } break;
  case VOLUME:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::VOLUME, event.value);
    break;
  case SWING: {
    float distance_from_center = fabsf(event.value - config::analog_controls::SWING_KNOB_CENTER_VALUE); // Range 0.0 to 0.5

    uint8_t swing_percent = config::analog_controls::SWING_BASE_PERCENT + static_cast<uint8_t>(distance_from_center * config::analog_controls::SWING_PERCENT_SENSITIVITY);

    bool delay_odd = (event.value > config::analog_controls::SWING_KNOB_CENTER_VALUE);
    controls->_sequencer_controller_ref.set_swing_target(delay_odd);

    controls->_sequencer_controller_ref.set_swing_percent(swing_percent);
    break;
  }
  case CRUSH:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::CRUSH_RATE, event.value);
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::CRUSH_DEPTH, event.value);
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
    break;
  }
  case PITCH1:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value, 0);
    break;
  case PITCH2:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value, 1);
    break;
  case PITCH3:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value, 2);
    break;
  case PITCH4:
    parent->parent_controls->_sound_router_ref.set_parameter(drum::Parameter::PITCH, event.value, 3);
    break;
  case SPEED: {
    float bpm = config::analog_controls::MIN_BPM_ADJUST + event.value * (config::analog_controls::MAX_BPM_ADJUST - config::analog_controls::MIN_BPM_ADJUST);
    controls->_internal_clock.set_bpm(bpm);
    break;
  }
  }
}

PizzaControls::PlaybuttonComponent::PlaybuttonComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      playbutton_reader{AnalogInMux16{PIN_ADC, analog_address_pins, PLAYBUTTON}},
      playbutton{
          playbutton_reader, 0, // id for the Drumpad instance
          config::drumpad::DEBOUNCE_PRESS_MS,
          config::drumpad::DEBOUNCE_RELEASE_MS,
          config::drumpad::HOLD_THRESHOLD_MS,
          config::drumpad::HOLD_REPEAT_DELAY_MS,
          config::drumpad::HOLD_REPEAT_INTERVAL_MS,
          config::drumpad::MIN_PRESSURE_VALUE,
          config::drumpad::MAX_PRESSURE_VALUE,
          config::drumpad::MIN_VELOCITY_VALUE,
          config::drumpad::MAX_VELOCITY_VALUE
      },
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
  // Release event is currently unused, no action needed.
}
} // namespace drum
