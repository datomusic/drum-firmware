#include "pizza_display.h"
#include "drum_pizza_hardware.h" // For LED array mapping

#include <array>
#include <cstddef> // For size_t
#include <algorithm> // For std::min

// Include necessary Pico SDK headers for GPIO and time
extern "C" {
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdio.h> // For printf in init
}

namespace PizzaExample {

// --- Internal Helper Functions/Types (Anonymous Namespace) ---
namespace {

enum class ExternalPinState {
  FLOATING,
  PULL_UP,
  PULL_DOWN,
  UNDETERMINED
};

ExternalPinState check_external_pin_state(std::uint32_t gpio, const char *name) {
  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);

  gpio_disable_pulls(gpio);
  sleep_us(10);
  bool initial_read = gpio_get(gpio);

  gpio_pull_up(gpio);
  sleep_us(10);
  bool pullup_read = gpio_get(gpio);

  gpio_pull_down(gpio);
  sleep_us(10);
  bool pulldown_read = gpio_get(gpio);

  ExternalPinState determined_state;
  const char *state_str;

  if (!initial_read && pullup_read && !pulldown_read) {
    determined_state = ExternalPinState::FLOATING;
    state_str = "Floating";
  } else if (initial_read && pullup_read && !pulldown_read) {
    determined_state = ExternalPinState::FLOATING;
    state_str = "Floating";
  } else if (!initial_read && !pullup_read) {
    determined_state = ExternalPinState::PULL_DOWN;
    state_str = "External Pull-down";
  } else if (initial_read && pulldown_read) {
    determined_state = ExternalPinState::PULL_UP;
    state_str = "External Pull-up";
  } else {
    determined_state = ExternalPinState::UNDETERMINED;
    state_str = "Undetermined / Inconsistent Reads";
  }

  printf("PizzaDisplay Init: Pin %lu (%s) external state check result: %s\n", gpio, name,
         state_str);

  gpio_disable_pulls(gpio);
  sleep_us(10);

  return determined_state;
}

} // anonymous namespace
// --- End Internal Helper Functions/Types ---
    
    
PizzaDisplay::PizzaDisplay()
    : _leds(PIN_LED_DATA, Musin::Drivers::RGBOrder::GRB, 255, // Initialize _leds
            0xffe080), // Initial brightness 255, will be adjusted in init
      note_colors({0xFF0000, 0xFF0020, 0xFF0040, 0xFF0060, 0xFF1010, 0xFF1020, 0xFF2040,
                   0xFF2060, 0x0000FF, 0x0028FF, 0x0050FF, 0x0078FF, 0x1010FF, 0x1028FF,
                   0x2050FF, 0x3078FF, 0x00FF00, 0x00FF1E, 0x00FF3C, 0x00FF5A, 0x10FF10,
                   0x10FF1E, 0x10FF3C, 0x20FF5A, 0xFFFF00, 0xFFE100, 0xFFC300, 0xFFA500,
                   0xFFFF20, 0xFFE120, 0xFFC320, 0xFFA520}) {
}

bool PizzaDisplay::init() {
  printf("PizzaDisplay: Initializing LEDs...\n");

  // Check LED data pin state to determine initial brightness
  ExternalPinState led_pin_state = check_external_pin_state(PIN_LED_DATA, "LED_DATA");
  uint8_t initial_brightness = (led_pin_state == ExternalPinState::PULL_UP) ? 100 : 255;
  printf("PizzaDisplay: Setting initial LED brightness to %u (based on pin state: %d)\n",
         initial_brightness, static_cast<int>(led_pin_state));
  _leds.set_brightness(initial_brightness); // Use _leds
    
  if (!_leds.init()) { // Use _leds
    printf("Error: Failed to initialize WS2812 LED driver!\n");
    return false;
  }

  // Enable LED power pin
  gpio_init(PIN_LED_ENABLE);
  gpio_set_dir(PIN_LED_ENABLE, GPIO_OUT); // Use GPIO_OUT for direction
  gpio_put(PIN_LED_ENABLE, 1);

  clear();
  show(); // Show the cleared state initially
  printf("PizzaDisplay: Initialization Complete.\n");
  return true;
}
    
void PizzaDisplay::show() {
  _leds.show(); // Use _leds
}
    
void PizzaDisplay::set_brightness(uint8_t brightness) {
  _leds.set_brightness(brightness); // Use _leds
  // Note: Brightness only affects subsequent set_pixel calls in the current WS2812 impl.
  // If immediate effect is desired, the buffer would need to be recalculated.
}
    
void PizzaDisplay::clear() {
  _leds.clear(); // Use _leds
}
    
void PizzaDisplay::set_led(uint32_t index, uint32_t color) {
  if (index < NUM_LEDS) {
    _leds.set_pixel(index, color); // Use _leds
  }
}
    
void PizzaDisplay::set_play_button_led(uint32_t color) {
  _leds.set_pixel(LED_PLAY_BUTTON, color); // Use _leds
}
    
uint32_t PizzaDisplay::get_note_color(uint8_t note_index) const {
  if (note_index < note_colors.size()) {
    return note_colors[note_index];
  }
  return 0; // Return black for invalid index
}

uint32_t PizzaDisplay::get_drumpad_led_index(uint8_t pad_index) const {
  switch (pad_index) {
  case 0:
    return LED_DRUMPAD_1;
  case 1:
    return LED_DRUMPAD_2;
  case 2:
    return LED_DRUMPAD_3;
  case 3:
    return LED_DRUMPAD_4;
  default:
    return NUM_LEDS; // Invalid index
  }
}

void PizzaDisplay::set_keypad_led(uint8_t row, uint8_t col, uint8_t intensity) {
  if (col >= 4)
    return; // Column 4 (sample select) has no direct LED in LED_ARRAY

  // Map row/col to the linear LED_ARRAY index
  // Keypad rows are 0-7 (bottom to top), cols 0-3 for sequencer LEDs
  // LED_ARRAY maps visually left-to-right, top-to-bottom (steps 1-8)
  // Keypad row 7 -> Step 1 (Indices 0-3 in LED_ARRAY)
  // Keypad row 0 -> Step 8 (Indices 28-31 in LED_ARRAY)
  uint8_t step_index = 7 - row; // Map row 7 to step 0, row 0 to step 7
  size_t array_index = step_index * 4 + col;

  if (array_index < std::size(LED_ARRAY)) {
    uint32_t led_index = LED_ARRAY[array_index];
    // Scale intensity (0-127) to color (e.g., white 0xRRGGBB)
    // Simple scaling: intensity * 2 maps 0-127 to 0-254
    // Scale intensity (0-127) to brightness (0-254), clamp at 255
    uint16_t calculated_brightness = static_cast<uint16_t>(intensity) * 2; // Calculate first
    uint8_t brightness_val = static_cast<uint8_t>(std::min(calculated_brightness, static_cast<uint16_t>(255))); // Now both args are uint16_t
    // Apply brightness to white (0xFFFFFF) using the new WS2812 method
    uint32_t color = leds.adjust_color_brightness(0xFFFFFF, brightness_val);
    leds.set_pixel(led_index, color);
  }
}

// --- Sequencer Display ---
// Needs to be defined before explicit instantiation below
template <size_t NumTracks, size_t NumSteps>
void PizzaDisplay::display_sequencer_state(
    const PizzaSequencer::Sequencer<NumTracks, NumSteps> &sequencer) {
  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    // Assuming track index maps directly to keypad column
    if (track_idx >= 4)
      continue; // Only display first 4 tracks on keypad cols 0-3

    const auto &track = sequencer.get_track(track_idx);
    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      // Assuming step index maps directly to keypad row (inverted)
      // Step 0 -> Row 7, Step 7 -> Row 0
      if (step_idx >= 8)
        continue; // Only display first 8 steps on keypad rows 0-7
      uint8_t row = 7 - step_idx;
      uint8_t col = track_idx;

     const auto &step = track.get_step(step_idx);

     uint32_t final_color = 0; // Default to black (off)

     if (step.enabled && step.note.has_value()) {
       uint32_t base_color = get_note_color(step.note.value() % 32); // Use modulo for safety

       // Determine brightness based on velocity
       uint8_t brightness = 255; // Default to full brightness
       if (step.velocity.has_value()) {
         // Scale velocity (1-127) to brightness (0-254 approx), clamp at 255
         uint16_t calculated_brightness = static_cast<uint16_t>(step.velocity.value()) * 2;
         brightness = static_cast<uint8_t>(std::min(calculated_brightness, static_cast<uint16_t>(255)));
       }

       // Apply brightness using the new WS2812 method
       final_color = leds.adjust_color_brightness(base_color, brightness);
     }
    
     // Map row/col to the linear LED_ARRAY index
     uint8_t led_array_index = (7 - row) * 4 + col; // Recalculate index based on row/col
     if (led_array_index < LED_ARRAY.size()) {      // Bounds check
       leds.set_pixel(LED_ARRAY[led_array_index], final_color);
     }
   }
  }
}

// Explicit template instantiation for the sequencer used in main.cpp
// This is necessary because the definition is in the .cpp file.
template void
PizzaDisplay::display_sequencer_state<4, 8>(const PizzaSequencer::Sequencer<4, 8> &sequencer);

} // namespace PizzaExample
