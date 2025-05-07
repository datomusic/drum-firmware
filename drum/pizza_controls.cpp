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
                             musin::timing::Sequencer<4, 8> &sequencer_ref,
                             musin::timing::InternalClock &clock_ref,
                             musin::timing::TempoHandler &tempo_handler_ref,
                             musin::timing::TempoMultiplier &tempo_multiplier_ref, // Added
                             drum::DefaultSequencerController &sequencer_controller_ref,
                             drum::SoundRouter &sound_router_ref)
    : display(display_ref), sequencer(sequencer_ref), _internal_clock(clock_ref),
      _tempo_handler_ref(tempo_handler_ref),
      _tempo_multiplier_ref(tempo_multiplier_ref), // Initialize
      _sequencer_controller_ref(sequencer_controller_ref), _sound_router_ref(sound_router_ref),
      keypad_component(this), drumpad_component(this), analog_component(this, _sound_router_ref),
      _profiler(2000), playbutton_component(this) {
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

  // Connect DrumpadComponent NoteEvents to SoundRouter
  drumpad_component.add_observer(_sound_router_ref);

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
    uint8_t brightness = static_cast<uint8_t>(_stopped_highlight_factor * 255.0f);
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
      constexpr uint8_t retrigger_velocity = 100;

      bool trigger_now = false;
      if (mode == musin::ui::RetriggerMode::Single) {
        if (_sub_step_tick_counter == 0) {
          trigger_now = true;
        }
      } else if (mode == musin::ui::RetriggerMode::Double) {
        if (_sub_step_tick_counter == 0 ||
            (retrigger_period >= 2 && _sub_step_tick_counter == (retrigger_period / 2))) {
          trigger_now = true;
        }
      }

      if (trigger_now) {
        drum::Events::NoteEvent note_event{
            .track_index = static_cast<uint8_t>(i), .note = note, .velocity = retrigger_velocity};
        _sound_router_ref.notification(note_event);
      }
    }
    _sub_step_tick_counter++;
    if (_sub_step_tick_counter >= retrigger_period) {
      _sub_step_tick_counter = 0;
    }
  }
}

// Notification handler for SequencerTickEvents from TempoMultiplier
void PizzaControls::notification(musin::timing::SequencerTickEvent /*event*/) {
  // _sub_step_tick_counter is now managed by the TempoEvent notification.
  // No action needed here regarding _sub_step_tick_counter.
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
  // Trigger fade only for note-on events (velocity > 0)
  // and if the track_index is valid for the drumpads.
  if (event.velocity > 0 && event.track_index < drumpad_component.get_num_drumpads()) {
    drumpad_component.trigger_fade(event.track_index);
  }
}

PizzaControls::KeypadComponent::KeypadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr), keypad(keypad_decoder_pins, keypad_columns_pins, 10, 5, 1000),
      keypad_observer(this, keypad_cc_map, 0) {
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
  if (event.col >= 4) {
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
            .velocity = 100 // Default preview velocity
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
      // Set default velocity only if it wasn't already set
      if (!track.get_step_velocity(step_idx).has_value()) {
        track.set_step_velocity(step_idx, 100);
        step_velocity = 100;
      } else {
        step_velocity = track.get_step_velocity(step_idx).value();
      }

      if (!controls->is_running()) {
        drum::Events::NoteEvent note_event{
            .track_index = track_idx, .note = note, .velocity = step_velocity};
        controls->_sound_router_ref.notification(note_event);
      }
    } else {
      // Optionally clear note/velocity when disabling, or leave them
      // track.set_step_note(step_idx, std::nullopt);
      // track.set_step_velocity(step_idx, std::nullopt);
    }
  } else if (event.type == musin::ui::KeypadEvent::Type::Hold) {
    // Set velocity to max on hold (only affects enabled steps implicitly via set_step_velocity)
    // Note: We might only want to do this if the step *is* enabled.
    // However, set_step_velocity doesn't check enabled status.
    // If the step was just enabled by the preceding Press event, this is fine.
    // If it was already enabled, this is also fine.
    // If it was disabled, setting velocity might be unwanted, but harmless for now.
    track.set_step_velocity(step_idx, 127);
  }
}

// --- DrumpadComponent Implementation ---

PizzaControls::DrumpadComponent::DrumpadComponent(PizzaControls *parent_ptr) // Removed sound_router
    : parent_controls(parent_ptr), // Removed _sound_router init
      drumpad_readers{AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_1},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_2},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_3},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_4}},
      drumpads{Drumpad<AnalogInMux16>{drumpad_readers[0], 0, 50U, 250U, 150U, 1500U, 100U, 800U,
                                      1000U, 5000U, 200000U},
               Drumpad<AnalogInMux16>{drumpad_readers[1], 1, 50U, 250U, 150U, 1500U, 100U, 800U,
                                      1000U, 5000U, 200000U},
               Drumpad<AnalogInMux16>{drumpad_readers[2], 2, 50U, 250U, 150U, 1500U, 100U, 800U,
                                      1000U, 5000U, 200000U},
               Drumpad<AnalogInMux16>{drumpad_readers[3], 3, 50U, 250U, 150U, 1500U, 100U, 800U,
                                      1000U, 5000U, 200000U}},
      drumpad_note_numbers{DrumpadComponent::drumpad_note_ranges[0].min_note,
                           DrumpadComponent::drumpad_note_ranges[1].min_note,
                           DrumpadComponent::drumpad_note_ranges[2].min_note,
                           DrumpadComponent::drumpad_note_ranges[3].min_note},
      // _pad_pressed_state is initialized by default in the header
      _fade_start_time{}, // Initialize before observers to match declaration order
      drumpad_observers{DrumpadEventHandler{this, 0},   // Removed sound_router
                        DrumpadEventHandler{this, 1},   // Removed sound_router
                        DrumpadEventHandler{this, 2},   // Removed sound_router
                        DrumpadEventHandler{this, 3}} { // Removed sound_router
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

    uint8_t note_index = drumpad_note_numbers[i];
    auto led_index_opt = controls->display.get_drumpad_led_index(i);

    if (led_index_opt.has_value()) {
      uint32_t led_index = led_index_opt.value();
      uint32_t base_color = controls->display.get_note_color(note_index);
      uint32_t final_color = base_color; // Default to base color

      // Check fade state
      if (!is_nil_time(_fade_start_time[i])) {
        uint64_t time_since_fade_start_us = absolute_time_diff_us(_fade_start_time[i], now);
        uint64_t fade_duration_us = static_cast<uint64_t>(FADE_DURATION_MS) * 1000;

        if (time_since_fade_start_us < fade_duration_us) {
          // Fade is active: Calculate brightness factor (MIN_FADE_BRIGHTNESS_FACTOR up to 1.0)
          float fade_progress = std::min(1.0f, static_cast<float>(time_since_fade_start_us) /
                                                   static_cast<float>(fade_duration_us));
          float current_brightness_factor =
              MIN_FADE_BRIGHTNESS_FACTOR + fade_progress * (1.0f - MIN_FADE_BRIGHTNESS_FACTOR);
          uint8_t brightness_value =
              static_cast<uint8_t>(std::clamp(current_brightness_factor * 255.0f, 0.0f, 255.0f));
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
  if (pad_index >= drumpad_note_numbers.size())
    return;

  const NoteRange &range = drumpad_note_ranges[pad_index];
  int32_t current_note = drumpad_note_numbers[pad_index];

  int32_t num_notes_in_range = range.max_note - range.min_note + 1;
  if (num_notes_in_range <= 0) {
    return;
  }

  int32_t current_note_in_range_idx = current_note - range.min_note;
  int32_t new_note_in_range_idx = (current_note_in_range_idx + offset);
  new_note_in_range_idx =
      (new_note_in_range_idx % num_notes_in_range + num_notes_in_range) % num_notes_in_range;

  drumpad_note_numbers[pad_index] = range.min_note + static_cast<uint8_t>(new_note_in_range_idx);

  // Update all steps in the corresponding sequencer track with the new note
  parent_controls->sequencer.get_track(pad_index).set_note(drumpad_note_numbers[pad_index]);

  auto led_index_opt = parent_controls->display.get_drumpad_led_index(pad_index);
  if (led_index_opt.has_value()) {
    uint32_t led_index = led_index_opt.value();
    uint32_t base_color = parent_controls->display.get_note_color(drumpad_note_numbers[pad_index]);
    // Just set the base color, brightness is handled by fade logic now
    parent_controls->display.set_led(led_index, base_color);
  }
  trigger_fade(pad_index); // Briefly flash when selecting
}

bool PizzaControls::DrumpadComponent::is_pad_pressed(uint8_t pad_index) const {
  if (pad_index < _pad_pressed_state.size()) {
    return _pad_pressed_state[pad_index];
  }
  return false;
}

uint8_t PizzaControls::DrumpadComponent::get_note_for_pad(uint8_t pad_index) const {
  if (pad_index < drumpad_note_numbers.size()) {
    return drumpad_note_numbers[pad_index];
  }
  return 36;
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
        parent->trigger_fade(event.pad_index); // Trigger fade on physical press
        uint8_t note = parent->get_note_for_pad(event.pad_index);
        uint8_t velocity = event.velocity.value();
        drum::Events::NoteEvent note_event{
            .track_index = event.pad_index, .note = note, .velocity = velocity};
        parent->notify_observers(note_event);
      }
    } else if (event.type == musin::ui::DrumpadEvent::Type::Release) {
      parent->_pad_pressed_state[event.pad_index] = false;
      uint8_t note = parent->get_note_for_pad(event.pad_index);
      drum::Events::NoteEvent note_event{
          .track_index = event.pad_index, .note = note, .velocity = 0};
      parent->notify_observers(note_event);
    }
  }
}

// --- AnalogControlComponent Implementation ---

PizzaControls::AnalogControlComponent::AnalogControlComponent(
    PizzaControls *parent_ptr,
    drum::SoundRouter &sound_router)                            // Added sound_router
    : parent_controls(parent_ptr), _sound_router(sound_router), // Store sound_router reference
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
          // AnalogControlEventHandler{this, DRUM1, _sound_router},      // Pass sound_router
          AnalogControlEventHandler{this, FILTER, _sound_router}, // Pass sound_router
          // AnalogControlEventHandler{this, DRUM2, _sound_router},      // Pass sound_router
          AnalogControlEventHandler{this, PITCH1, _sound_router}, // Pass sound_router
          AnalogControlEventHandler{this, PITCH2, _sound_router}, // Pass sound_router
          // AnalogControlEventHandler{this, PLAYBUTTON, _sound_router}, // Pass sound_router
          AnalogControlEventHandler{this, RANDOM, _sound_router}, // Pass sound_router
          AnalogControlEventHandler{this, VOLUME, _sound_router}, // Pass sound_router
          AnalogControlEventHandler{this, PITCH3, _sound_router}, // Pass sound_router
          AnalogControlEventHandler{this, SWING, _sound_router},  // Pass sound_router
          AnalogControlEventHandler{this, CRUSH, _sound_router},  // Pass sound_router
          // AnalogControlEventHandler{this, DRUM3, _sound_router},      // Pass sound_router
          AnalogControlEventHandler{this, REPEAT, _sound_router}, // Pass sound_router
          // AnalogControlEventHandler{this, DRUM4, _sound_router},      // Pass sound_router
          AnalogControlEventHandler{this, SPEED, _sound_router},    // Pass sound_router
          AnalogControlEventHandler{this, PITCH4, _sound_router}} { // Pass sound_router
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
    _sound_router.set_parameter(drum::Parameter::FILTER_FREQUENCY, event.value);
    _sound_router.set_parameter(drum::Parameter::FILTER_RESONANCE, (1.0f - event.value));
    break;
  case RANDOM: {
    constexpr float RANDOM_THRESHOLD = 0.1f;
    bool was_active = controls->_sequencer_controller_ref.is_random_active();
    bool should_be_active = (event.value >= RANDOM_THRESHOLD);

    if (should_be_active && !was_active) {
      controls->_sequencer_controller_ref.activate_random();
    } else if (!should_be_active && was_active) {
      controls->_sequencer_controller_ref.deactivate_random();
    }
  } break;
  case VOLUME:
    _sound_router.set_parameter(drum::Parameter::VOLUME, event.value);
    break;
  case SWING: {
    constexpr float center_value = 0.5f;
    float distance_from_center = fabsf(event.value - center_value); // Range 0.0 to 0.5

    uint8_t swing_percent = 50 + static_cast<uint8_t>(distance_from_center * 33.0f);

    bool delay_odd = (event.value > center_value);
    controls->_sequencer_controller_ref.set_swing_target(delay_odd);

    controls->_sequencer_controller_ref.set_swing_percent(swing_percent);
    break;
  }
  case CRUSH:
    _sound_router.set_parameter(drum::Parameter::CRUSH_RATE, event.value);
    //_sound_router.set_parameter(drum::Parameter::CRUSH_DEPTH, event.value);
    break;
  case REPEAT: {
    constexpr float REPEAT_THRESHOLD_1 = 0.3f;
    constexpr float REPEAT_THRESHOLD_2 = 0.7f;
    constexpr uint32_t REPEAT_LENGTH_1 = 3; // Length for range [T1, T2)
    constexpr uint32_t REPEAT_LENGTH_2 = 1; // Length for range [T2, 1.0]

    // Determine the intended state based on the knob value
    std::optional<uint32_t> intended_length = std::nullopt;
    if (event.value >= REPEAT_THRESHOLD_2) {
      intended_length = REPEAT_LENGTH_2;
    } else if (event.value >= REPEAT_THRESHOLD_1) {
      intended_length = REPEAT_LENGTH_1;
    }

    // Pass the intended state to the sequencer controller
    controls->_sequencer_controller_ref.set_intended_repeat_state(intended_length);

    // Note: We no longer send a generic REPEAT CC via SoundRouter here,
    // as the effect is handled internally by the SequencerController.
    // If a MIDI CC for repeat *is* desired, it would need a specific Parameter.
    break;
  }
  case DRUM1:
    _sound_router.set_parameter(drum::Parameter::DRUM_PRESSURE_1, event.value, 0);
    break;
  case DRUM2:
    _sound_router.set_parameter(drum::Parameter::DRUM_PRESSURE_2, event.value, 1);
    break;
  case DRUM3:
    _sound_router.set_parameter(drum::Parameter::DRUM_PRESSURE_3, event.value, 2);
    break;
  case DRUM4:
    _sound_router.set_parameter(drum::Parameter::DRUM_PRESSURE_4, event.value, 3);
    break;
  case PITCH1:
    _sound_router.set_parameter(drum::Parameter::PITCH, event.value, 0);
    break;
  case PITCH2:
    _sound_router.set_parameter(drum::Parameter::PITCH, event.value, 1);
    break;
  case PITCH3:
    _sound_router.set_parameter(drum::Parameter::PITCH, event.value, 2);
    break;
  case PITCH4:
    _sound_router.set_parameter(drum::Parameter::PITCH, event.value, 3);
    break;
  case SPEED: {
    constexpr float min_bpm = 30.0f;
    constexpr float max_bpm = 480.0f;
    float bpm = min_bpm + event.value * (max_bpm - min_bpm);
    controls->_internal_clock.set_bpm(bpm);
    break;
  }
  }
}

PizzaControls::PlaybuttonComponent::PlaybuttonComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      playbutton_reader{AnalogInMux16{PIN_ADC, analog_address_pins, PLAYBUTTON}},
      playbutton{
          Drumpad<AnalogInMux16>{playbutton_reader, 0, 50U, 250U, 150U, 1500U, 100U, 800U, 1000U,
                                 5000U, 200000U},
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
