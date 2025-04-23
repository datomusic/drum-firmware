#include "pizza_controls.h"
#include "midi.h"          // For send_midi_cc, send_midi_note
#include "pizza_display.h" // Need definition for display methods
#include "sequencer.h"     // Need definition for Sequencer
#include <algorithm>       // For std::clamp
#include <cmath>           // For std::max used in scaling
#include <cstddef>         // For size_t
#include <cstdio>          // For printf

using Musin::HAL::AnalogInMux16;
using Musin::UI::AnalogControl;
using Musin::UI::Drumpad;

// --- Constructor ---
// Use PizzaExample::PizzaDisplay for the parameter type
PizzaControls::PizzaControls(PizzaExample::PizzaDisplay &display_ref,
                             PizzaSequencer::Sequencer<4, 8> &sequencer_ref) // Accept sequencer ref
    : display(display_ref), sequencer(sequencer_ref), // Store references (display is now PizzaExample::PizzaDisplay&)
      keypad(keypad_decoder_pins, keypad_columns_pins, 10, 5, 1000),
      keypad_observer(this, keypad_cc_map, 0), // Pass parent pointer and map reference
      drumpad_readers{// Initialize readers directly by calling constructors
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_1},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_2},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_3},
                      AnalogInMux16{PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_4}},
      drumpads{// Initialize drumpads using the readers by calling constructors explicitly
               Musin::UI::Drumpad<AnalogInMux16>{drumpad_readers[0], 0, 50U, 250U, 150U, 3000U, 100U,
                                                 800U, 1000U, 5000U, 200000U}, // Pad index 0
               Musin::UI::Drumpad<AnalogInMux16>{drumpad_readers[1], 1, 50U, 250U, 150U, 3000U, 100U,
                                                 800U, 1000U, 5000U, 200000U}, // Pad index 1
               Musin::UI::Drumpad<AnalogInMux16>{drumpad_readers[2], 2, 50U, 250U, 150U, 3000U, 100U,
                                                 800U, 1000U, 5000U, 200000U}, // Pad index 2
               Musin::UI::Drumpad<AnalogInMux16>{drumpad_readers[3], 3, 50U, 250U, 150U, 3000U, 100U,
                                                 800U, 1000U, 5000U, 200000U}}, // Pad index 3
      drumpad_note_numbers{0, 7, 15, 23}, // Initial notes
      mux_controls{                       // Initialize by explicitly calling constructors
                   // Assuming order matches the enum in drum_pizza_hardware.h
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM1, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, FILTER, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM2, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH1, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH2, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PLAYBUTTON, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, RANDOM, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, VOLUME},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH3, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SWING},
                   AnalogControl{PIN_ADC, analog_address_pins, CRUSH, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM3, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, REPEAT, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, DRUM4, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, SPEED, 0.005f, true},
                   AnalogControl{PIN_ADC, analog_address_pins, PITCH4, 0.005f, true}},
      control_observers{// Initialize observers by explicitly calling constructors (id, cc, channel)
                        AnalogControlEventHandler{this, DRUM1, DRUM1, 0},
                        AnalogControlEventHandler{this, FILTER, 75, 0},
                        AnalogControlEventHandler{this, DRUM2, DRUM2, 0},
                        AnalogControlEventHandler{this, PITCH1, 16, 1},
                        AnalogControlEventHandler{this, PITCH2, 17, 2},
                        AnalogControlEventHandler{this, PLAYBUTTON, PLAYBUTTON, 0},
                        AnalogControlEventHandler{this, RANDOM, RANDOM, 0},
                        AnalogControlEventHandler{this, VOLUME, VOLUME, 0},
                        AnalogControlEventHandler{this, PITCH3, 18, 3},
                        AnalogControlEventHandler{this, SWING, SWING, 0},
                        AnalogControlEventHandler{this, CRUSH, 77, 0},
                        AnalogControlEventHandler{this, DRUM3, DRUM3, 0},
                        AnalogControlEventHandler{this, REPEAT, REPEAT, 0},
                        AnalogControlEventHandler{this, DRUM4, DRUM4, 0},
                        AnalogControlEventHandler{this, SPEED, SPEED, 0},
                        AnalogControlEventHandler{this, PITCH4, 19, 4}},
      drumpad_observers{// Initialize drumpad observers
                        DrumpadEventHandler{this, 0}, DrumpadEventHandler{this, 1},
                        DrumpadEventHandler{this, 2}, DrumpadEventHandler{this, 3}} {
}

// --- Initialization ---
void PizzaControls::init() {
  printf("PizzaControls: Initializing...\n");

  // Initialize Keypad
  this->keypad.init();
  this->keypad.add_observer(keypad_observer);
  printf("PizzaControls: Keypad Initialized (%u rows, %u cols)\n", this->keypad.get_num_rows(),
         this->keypad.get_num_cols());

  // Initialize Drumpad Readers
  for (auto &reader : drumpad_readers) {
    reader.init();
  }
  // Drumpad objects themselves don't need init() as they use initialized readers
  printf("PizzaControls: Drumpad Readers Initialized\n");

  // Attach Drumpad Observers
  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].add_observer(drumpad_observers[i]); // Now uncommented
  }
  printf("PizzaControls: Drumpad Observers Attached\n");

  // Initialize Analog Controls and attach observers
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].init();
    mux_controls[i].add_observer(control_observers[i]);
  }
  printf("PizzaControls: Initialized %zu analog controls\n", mux_controls.size());

  printf("PizzaControls: Initialization Complete.\n");
}

// --- Update ---
void PizzaControls::update() {
  // Update all analog mux controls - observers will be notified automatically
  for (auto &control : mux_controls) {
    control.update();
  }

  // Scan the keypad - observers will be notified automatically
  this->keypad.scan();

  // Update drumpads and handle MIDI/Display updates
  update_drumpads();

  // Display updates are requested within observers and update_drumpads
  // The actual display.show() is called in main.cpp's loop
}
    
// --- Private Methods ---
    
float PizzaControls::scale_raw_to_brightness(uint16_t raw_value) const {
  // Map ADC range (e.g., 100-1000) to brightness (e.g., 0.1-1.0)
  // Adjust these based on sensor readings and desired visual response
  constexpr uint16_t min_adc = 100;
  constexpr uint16_t max_adc = 4095;
  constexpr float min_brightness = 0.1f;
  constexpr float max_brightness = 1.0f;

  if (raw_value <= min_adc) {
    return max_brightness; // Max brightness at minimum pressure (or below)
  }
  if (raw_value >= max_adc) {
    return min_brightness; // Minimum brightness at maximum pressure (or above) - Changed from 0.0f
  }

  // Inverted linear scaling: factor decreases as raw_value increases
  float factor = static_cast<float>(max_adc - raw_value) / (max_adc - min_adc);
  return min_brightness + factor * (max_brightness - min_brightness); // Apply the inverted factor
}

uint32_t PizzaControls::calculate_brightness_color(uint32_t base_color, uint16_t raw_value) const {
  if (base_color == 0)
    return 0;
    
  float brightness_factor = scale_raw_to_brightness(raw_value);
  // Convert float factor (0.0-1.0) to uint8_t brightness (0-255)
  uint8_t brightness_val = static_cast<uint8_t>(std::clamp(brightness_factor * 255.0f, 0.0f, 255.0f));
    
  // Use the display's leds object and the new method
  // Note: display.leds() requires the accessor added in pizza_display.h
  return display.leds().adjust_color_brightness(base_color, brightness_val);
}
    
void PizzaControls::update_drumpads() {
  for (size_t i = 0; i < drumpads.size(); ++i) {
    drumpads[i].update(); // Update call remains, event handling moves to observer

    // Keep LED update based on current pressure for now
    // This might also move to the observer later depending on desired behavior
    uint16_t raw_value = drumpads[i].get_raw_adc_value();
    uint8_t note_index = drumpad_note_numbers[i];
    uint32_t led_index = display.get_drumpad_led_index(i);
    if (led_index < NUM_LEDS) {
      uint32_t base_color = display.get_note_color(note_index);
      uint32_t final_color = calculate_brightness_color(base_color, raw_value);
      display.set_led(led_index, final_color);
    }
  }
}

void PizzaControls::select_note_for_pad(uint8_t pad_index, int8_t offset) {
  if (pad_index >= drumpad_note_numbers.size())
    return;

  int32_t current_note = drumpad_note_numbers[pad_index];
  int32_t new_note_number = current_note + offset;

  // Wrap around 0-31 range
  if (new_note_number < 0) {
    new_note_number = 31;
  } else if (new_note_number > 31) {
    new_note_number = 0;
  }
  drumpad_note_numbers[pad_index] = static_cast<uint8_t>(new_note_number);

  // Update the display for the affected pad immediately
  uint32_t led_index = display.get_drumpad_led_index(pad_index);
  if (led_index < NUM_LEDS) {
    uint32_t base_color = display.get_note_color(drumpad_note_numbers[pad_index]);
    // When selecting a note, show the base color at maximum brightness (inverted logic)
    // Use calculate_brightness with a low raw value (min_adc) which now maps to max brightness.
    uint32_t final_color = calculate_brightness_color(base_color, 100);
    display.set_led(led_index, final_color);
  }
}

// --- Observer Implementations ---

// Stub implementation for DrumpadEventHandler
void PizzaControls::DrumpadEventHandler::notification(Musin::UI::DrumpadEvent event) {
  // TODO: Implement drumpad event handling logic here
  // - Send MIDI notes based on event.type (Press/Release) and event.velocity
  // - Potentially update LEDs based on events (e.g., flash on press)
  // printf("Drumpad Event: Pad %u, Type %d, Vel %u, Raw %u\n",
  //        event.pad_index, static_cast<int>(event.type),
  //        event.velocity.value_or(0), event.raw_value);
}

void PizzaControls::AnalogControlEventHandler::notification(Musin::UI::AnalogControlEvent event) {
  // Access parent members via parent pointer
  uint8_t value = static_cast<uint8_t>(event.value * 127.0f);

  switch (control_id) {
  case PLAYBUTTON:
    // Update Play button LED via parent's display reference
    parent->display.set_play_button_led((static_cast<uint32_t>(value * 2) << 16) |
                                        (static_cast<uint32_t>(value * 2) << 8) | (value * 2));
    break;
  default:
    // Send other CCs on channel 1 (or adjust as needed)
    send_midi_cc(1, cc_number, value);
    break;
  }
}

void PizzaControls::KeypadEventHandler::notification(Musin::UI::KeypadEvent event) {
  // Access parent members via parent pointer

  // --- Handle Sample Select (Column 4) ---
  if (event.col >= 4) {
    if (event.type == Musin::UI::KeypadEvent::Type::Press) {
      // Map row to pad index (row 7 -> pad 0, row 0 -> pad 3)
      uint8_t pad_index = 0;
      int8_t offset = 0;
      switch (event.row) {
      case 0:
        pad_index = 3;
        offset = -1;
        break; // Bottom pair -> Pad 4
      case 1:
        pad_index = 3;
        offset = 1;
        break;
      case 2:
        pad_index = 2;
        offset = -1;
        break; // Next pair -> Pad 3
      case 3:
        pad_index = 2;
        offset = 1;
        break;
      case 4:
        pad_index = 1;
        offset = -1;
        break; // Next pair -> Pad 2
      case 5:
        pad_index = 1;
        offset = 1;
        break;
      case 6:
        pad_index = 0;
        offset = -1;
        break; // Top pair -> Pad 1
      case 7:
        pad_index = 0;
        offset = 1;
        break;
      }
      parent->select_note_for_pad(pad_index, offset);
    }
    // No further action needed for sample select column after Press
    return; // Exit early for column 4
  }

  // --- Sequencer Step Toggling Logic (Columns 0-3) ---
  if (event.type == Musin::UI::KeypadEvent::Type::Press) {
    uint8_t track_idx = event.col;
    uint8_t step_idx = 7 - event.row; // Map row 0-7 to step 7-0

    // Access the step in the sequencer via the parent pointer
    PizzaSequencer::Step &step = parent->sequencer.get_track(track_idx).get_step(step_idx);

    // Toggle enabled state
    step.enabled = !step.enabled;

    // If step is now enabled, always assign the current pad note and default velocity if needed
    if (step.enabled) {
      // Ensure track_idx is valid for drumpad_note_numbers before assigning
      if (track_idx < parent->drumpad_note_numbers.size()) {
        step.note = parent->drumpad_note_numbers[track_idx]; // Always set the note
      } else {
        step.note = 36; // Fallback note if track index is somehow invalid
      }

      // Assign default velocity only if none exists
      if (!step.velocity.has_value()) {
        step.velocity = 100; // Default velocity
      }
    }
    // Note: LED update is handled by display_sequencer_state in the main loop
  } else if (event.type == Musin::UI::KeypadEvent::Type::Hold) {
    // --- Handle Hold for Sequencer Steps (Set Max Velocity) ---
    uint8_t track_idx = event.col;
    uint8_t step_idx = 7 - event.row; // Map row 0-7 to step 7-0

    // Access the step in the sequencer via the parent pointer
    PizzaSequencer::Step &step = parent->sequencer.get_track(track_idx).get_step(step_idx);

    // Set velocity to maximum only if the step is enabled
    if (step.enabled) {
      step.velocity = 127;
      // Optionally, add a visual indicator via display if needed,
      // though display_sequencer_state might handle it if it shows velocity.
    }
  }
  // Ignore Release events for sequencer columns for now
}
