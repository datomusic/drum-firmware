#include "pizza_controls.h"
#include "midi.h"           // For send_midi_cc, send_midi_note
#include "pizza_display.h"  // Need definition for display methods
#include "step_sequencer.h" // Need definition for Sequencer
#include <algorithm>        // For std::clamp, std::min
#include <cmath>            // For std::max, std::round used in scaling
#include <cstddef>          // For size_t
#include <cstdio>           // For printf

using Musin::HAL::AnalogInMux16;
using Musin::UI::AnalogControl;
using Musin::UI::Drumpad;

PizzaControls::PizzaControls(PizzaExample::PizzaDisplay &display_ref,
                             StepSequencer::Sequencer<4, 8> &sequencer_ref,
                             Clock::InternalClock &clock_ref)
    : display(display_ref), sequencer(sequencer_ref), _internal_clock(clock_ref),
      keypad_component(this), drumpad_component(this), analog_component(this),
      playbutton_component(this) {
}

void PizzaControls::init() {
  keypad_component.init();
  drumpad_component.init();
  analog_component.init();
  playbutton_component.init();
}

void PizzaControls::update() {
  keypad_component.update();
  drumpad_component.update();
  analog_component.update();
  playbutton_component.update();
}

PizzaControls::KeypadComponent::KeypadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr), keypad(keypad_decoder_pins, keypad_columns_pins, 10, 5, 1000),
      keypad_observer(this, keypad_cc_map, 0)
{
}

void PizzaControls::KeypadComponent::init() {
  keypad.init();
  keypad.add_observer(keypad_observer);
}

void PizzaControls::KeypadComponent::update() {
  keypad.scan();
}

void PizzaControls::KeypadComponent::KeypadEventHandler::notification(
    Musin::UI::KeypadEvent event) {
  PizzaControls *controls = parent->parent_controls;

  // Sample Select (Column 4)
  if (event.col >= 4) {
    if (event.type == Musin::UI::KeypadEvent::Type::Press) {
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
    }
    return;
  }

  // Sequencer Step Toggling (Columns 0-3)
  if (event.type == Musin::UI::KeypadEvent::Type::Press) {
    uint8_t track_idx = event.col;
    uint8_t step_idx = 7 - event.row; // Map row 7 (top) to step 0

    StepSequencer::Step &step = controls->sequencer.get_track(track_idx).get_step(step_idx);
    step.enabled = !step.enabled;

    if (step.enabled) {
      // Get the current note assigned to the corresponding drumpad
      step.note = controls->drumpad_component.get_note_for_pad(track_idx);
      if (!step.velocity.has_value()) {
        step.velocity = 100; // Default velocity if none set
      }
    }
    // LED update is handled by PizzaDisplay::draw_sequencer_state called in main loop
  } else if (event.type == Musin::UI::KeypadEvent::Type::Hold) {
    // Hold Sequencer Step -> Set Max Velocity
    uint8_t track_idx = event.col;
    uint8_t step_idx = 7 - event.row;
    StepSequencer::Step &step = controls->sequencer.get_track(track_idx).get_step(step_idx);
    if (step.enabled) {
      step.velocity = 127;
    }
  }
}

// --- DrumpadComponent Implementation ---

PizzaControls::DrumpadComponent::DrumpadComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
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
      drumpad_note_numbers{0, 7, 15, 23}, // Default notes
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
  update_drumpads(); // Call the helper method
}

void PizzaControls::DrumpadComponent::update_drumpads() {
  PizzaControls *controls = parent_controls; // Access shared resources
  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].update();

    // Update LED based on current pressure
    uint16_t raw_value = drumpads[i].get_raw_adc_value();
    uint8_t note_index = drumpad_note_numbers[i];
    uint32_t led_index = controls->display.get_drumpad_led_index(i);

    if (led_index < NUM_LEDS) { // Check if index is valid
      uint32_t base_color = controls->display.get_note_color(note_index);
      uint32_t final_color = calculate_brightness_color(base_color, raw_value);
      controls->display.set_led(led_index, final_color);
    }
  }
}

float PizzaControls::DrumpadComponent::scale_raw_to_brightness(uint16_t raw_value) const {
  constexpr uint16_t min_adc = 100;
  constexpr uint16_t max_adc = 4095;
  constexpr float min_brightness = 0.1f;
  constexpr float max_brightness = 1.0f;

  if (raw_value <= min_adc) {
    return max_brightness;
  }
  if (raw_value >= max_adc) {
    return min_brightness;
  }

  float factor = static_cast<float>(max_adc - raw_value) / (max_adc - min_adc);
  return min_brightness + factor * (max_brightness - min_brightness);
}

uint32_t PizzaControls::DrumpadComponent::calculate_brightness_color(uint32_t base_color,
                                                                     uint16_t raw_value) const {
  if (base_color == 0)
    return 0;

  float brightness_factor = scale_raw_to_brightness(raw_value);
  uint8_t brightness_val =
      static_cast<uint8_t>(std::clamp(brightness_factor * 255.0f, 0.0f, 255.0f));

  // Access display via parent_controls
  return parent_controls->display.leds().adjust_color_brightness(base_color, brightness_val);
}

void PizzaControls::DrumpadComponent::select_note_for_pad(uint8_t pad_index, int8_t offset) {
  if (pad_index >= drumpad_note_numbers.size())
    return;

  int32_t current_note = drumpad_note_numbers[pad_index];
  int32_t new_note_number = current_note + offset;

  if (new_note_number < 0) {
    new_note_number = 31;
  } else if (new_note_number > 31) {
    new_note_number = 0;
  }
  drumpad_note_numbers[pad_index] = static_cast<uint8_t>(new_note_number);

  // Update the corresponding track's note in the sequencer
  parent_controls->sequencer.get_track(pad_index).set_all_notes(drumpad_note_numbers[pad_index]);

  uint32_t led_index = parent_controls->display.get_drumpad_led_index(pad_index);
  if (led_index < NUM_LEDS) {
    uint32_t base_color = parent_controls->display.get_note_color(drumpad_note_numbers[pad_index]);
    // Show selected note color at max brightness (use min ADC value for scaling)
    uint32_t final_color = calculate_brightness_color(base_color, 100);
    parent_controls->display.set_led(led_index, final_color);
  }
}

uint8_t PizzaControls::DrumpadComponent::get_note_for_pad(uint8_t pad_index) const {
  if (pad_index < drumpad_note_numbers.size()) {
    return drumpad_note_numbers[pad_index];
  }
  return 36; // Default fallback note
}

void PizzaControls::DrumpadComponent::DrumpadEventHandler::notification(
    Musin::UI::DrumpadEvent event) {
  // Access shared resources via parent->parent_controls
  // PizzaControls* controls = parent->parent_controls;

  if (event.type == Musin::UI::DrumpadEvent::Type::Press && event.velocity.has_value()) {
    uint8_t note = parent->get_note_for_pad(event.pad_index);
    uint8_t velocity = event.velocity.value();
    send_midi_note(1, note, velocity); // Assuming channel 0 for now
  } else if (event.type == Musin::UI::DrumpadEvent::Type::Release) {
    uint8_t note = parent->get_note_for_pad(event.pad_index);
    send_midi_note(1, note, 0); // Note off
  }

}

// --- AnalogControlComponent Implementation ---

PizzaControls::AnalogControlComponent::AnalogControlComponent(PizzaControls *parent_ptr)
    : parent_controls(parent_ptr),
      mux_controls{AnalogControl{PIN_ADC, analog_address_pins, DRUM1, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, FILTER, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM2, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH1, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH2, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PLAYBUTTON, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, RANDOM, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, VOLUME},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH3, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SWING, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, CRUSH, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM3, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, REPEAT, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM4, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SPEED, 0.005f, false},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH4, 0.005f, true}},
      control_observers{
          AnalogControlEventHandler{this, DRUM1},  AnalogControlEventHandler{this, FILTER},
          AnalogControlEventHandler{this, DRUM2},  AnalogControlEventHandler{this, PITCH1},
          AnalogControlEventHandler{this, PITCH2}, AnalogControlEventHandler{this, PLAYBUTTON},
          AnalogControlEventHandler{this, RANDOM}, AnalogControlEventHandler{this, VOLUME},
          AnalogControlEventHandler{this, PITCH3}, AnalogControlEventHandler{this, SWING},
          AnalogControlEventHandler{this, CRUSH},  AnalogControlEventHandler{this, DRUM3},
          AnalogControlEventHandler{this, REPEAT}, AnalogControlEventHandler{this, DRUM4},
          AnalogControlEventHandler{this, SPEED},  AnalogControlEventHandler{this, PITCH4}} {
}

void PizzaControls::AnalogControlComponent::init() {
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].init();
    mux_controls[i].add_observer(control_observers[i]);
  }
}

void PizzaControls::AnalogControlComponent::update() {
  for (auto &control : mux_controls) {
    control.update();
  }
}

void PizzaControls::AnalogControlComponent::AnalogControlEventHandler::notification(
    Musin::UI::AnalogControlEvent event) {
  PizzaControls *controls = parent->parent_controls;
  uint8_t midi_value = static_cast<uint8_t>(std::round(event.value * 127.0f));

  // Extract mux channel from packed control_id (upper 8 bits)
  const uint8_t mux_channel = event.control_id >> 8;

  switch (mux_channel) {
  case DRUM1:
    send_midi_cc(1, 20, midi_value);
    break;
  case FILTER:
    send_midi_cc(1, 75, midi_value);
    break;
  case DRUM2:
    send_midi_cc(1, 21, midi_value);
    break;
  case RANDOM:
    send_midi_cc(1, 79, midi_value);
    break;
  case VOLUME:
    send_midi_cc(1, 7, midi_value);
    break;
  case SWING:
    send_midi_cc(1, 80, midi_value);
    break;
  case CRUSH:
    send_midi_cc(1, 77, midi_value);
    break;
  case DRUM3:
    send_midi_cc(1, 22, midi_value);
    break;
  case REPEAT:
    send_midi_cc(1, 78, midi_value);
    break;
  case DRUM4:
    send_midi_cc(1, 23, midi_value);
    break;
  case PITCH1:
    send_midi_cc(1, 16, midi_value);
    break;
  case PITCH2:
    send_midi_cc(2, 17, midi_value);
    break;
  case PITCH3:
    send_midi_cc(3, 18, midi_value);
    break;
  case PITCH4:
    send_midi_cc(4, 19, midi_value);
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
    : parent_controls(parent_ptr), playbutton_reader{AnalogInMux16{PIN_ADC, analog_address_pins, PLAYBUTTON}},
      playbutton{Drumpad<AnalogInMux16>{playbutton_reader, 0, 50U, 250U, 150U, 1500U, 100U, 800U,
      1000U, 5000U, 200000U},},
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
  Musin::UI::DrumpadEvent event) {

    if (event.type == Musin::UI::DrumpadEvent::Type::Press) {
      printf("Playbutton pressed\n");
    } else if (event.type == Musin::UI::DrumpadEvent::Type::Release) {
      printf("Playbutton released\n");
    }
    // TODO: Toggle sequencer playing state
}
