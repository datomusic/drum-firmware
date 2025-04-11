// static_example_main.cpp
#include "pico/stdlib.h"
#include <cstdint>
#include <cstdio>
#include "pico/time.h"
#include <array>

// Include the specific MIDI observer implementation for this experiment
#include "midi_cc_observer.h"
// Include the core AnalogControl class from the musin library
#include "musin/ui/analog_control.h"
// Include the Keypad driver
#include "musin/ui/keypad_hc138.h"
#include "etl/span.h" // Required for keypad buffer span

using Musin::UI::AnalogControl;
using Musin::UI::Keypad_HC138;
using Musin::UI::KeyData;

extern "C" {
  #include "hardware/adc.h"
  #include "hardware/gpio.h"
}

constexpr auto PIN_ADDR_0 = 29;
constexpr auto PIN_ADDR_1 = 6;
constexpr auto PIN_ADDR_2 = 7;
constexpr auto PIN_ADDR_3 = 9;

constexpr auto PIN_ADC = 28;

constexpr unsigned int PIN_RING_1 = 15;
constexpr unsigned int PIN_RING_2 = 14;
constexpr unsigned int PIN_RING_3 = 13;
constexpr unsigned int PIN_RING_4 = 11;
constexpr unsigned int PIN_RING_5 = 10;

// Static array for multiplexer address pins (AnalogControls use 4)
const std::array<std::uint32_t, 4> analog_address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2, PIN_ADDR_3};
// Static array for keypad column pins
const std::array<uint, 5> keypad_columns_pins = {PIN_RING_1, PIN_RING_2, PIN_RING_3, PIN_RING_4, PIN_RING_5};
// Static array for keypad decoder address pins (uses first 3)
const std::array<uint, 3> keypad_decoder_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2};

// --- Keypad Configuration ---
constexpr uint8_t KEYPAD_ROWS = 8; // Using 3 address pins allows up to 8 rows
constexpr uint8_t KEYPAD_COLS = 5; // Based on keypad_columns_pins size
constexpr size_t KEYPAD_TOTAL_KEYS = KEYPAD_ROWS * KEYPAD_COLS;

// Static buffer for keypad key states
static std::array<KeyData, KEYPAD_TOTAL_KEYS> keypad_key_data_buffer;

// Static instance of the keypad driver
static Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad(
    keypad_decoder_pins,
    keypad_columns_pins,
    etl::span<KeyData>(keypad_key_data_buffer) // Create span from the buffer
);
// --- End Keypad Configuration ---


void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("MIDI CC %u: %u\n", cc_number, value);
}

// Define MIDI observers statically
// These will be allocated at compile time
static MIDICCObserver cc_observers[] = {
  {16, 0, send_midi_cc},  // CC 16, channel 1
  {17, 0, send_midi_cc},  // CC 17, channel 1
  {18, 0, send_midi_cc},  // CC 18, channel 1
  {19, 0, send_midi_cc},  // CC 19, channel 1
  {20, 0, send_midi_cc},  // CC 20, channel 1
  {21, 0, send_midi_cc},  // CC 21, channel 1
  {22, 0, send_midi_cc},  // CC 22, channel 1
  {23, 0, send_midi_cc},  // CC 23, channel 1
  {24, 0, send_midi_cc}   // CC 24, channel 1
};

// Statically allocate multiplexed controls using the class from musin::ui
static AnalogControl<1> mux_controls[8] = {
  {10, PIN_ADC, analog_address_pins, 3 }, // ID 10, Mux Channel 3
  {11, PIN_ADC, analog_address_pins, 4 }, // ID 11, Mux Channel 4
  {12, PIN_ADC, analog_address_pins, 8 }, // ID 12, Mux Channel 8
  {13, PIN_ADC, analog_address_pins, 15 }, // ID 13, Mux Channel 15
  {14, PIN_ADC, analog_address_pins, 0 }, // ID 14, Mux Channel 0 (Example)
  {15, PIN_ADC, analog_address_pins, 1 }, // ID 15, Mux Channel 1 (Example)
  {16, PIN_ADC, analog_address_pins, 2 }, // ID 16, Mux Channel 2 (Example)
  {17, PIN_ADC, analog_address_pins, 5 }  // ID 17, Mux Channel 5 (Example) - Corrected index
};


int main() {
  // Initialize system
  stdio_init_all();
  
  sleep_ms(1000);

  printf("MIDI CC and Keypad demo\n");

  // Initialize Keypad
  keypad.init();
  printf("Keypad Initialized (%u rows, %u cols)\n", KEYPAD_ROWS, KEYPAD_COLS);

  // Initialize Analog Controls
  for (int i = 0; i < 8; i++) {
    mux_controls[i].init();
    mux_controls[i].add_observer(&cc_observers[i]);
  }

  while (true) {
      // Update all mux controls
      for (auto& control : mux_controls) {
          control.update();
      }

      // Scan the keypad
      if (keypad.scan()) {
          // Check each key if a scan happened
          for (uint8_t r = 0; r < KEYPAD_ROWS; ++r) {
              for (uint8_t c = 0; c < KEYPAD_COLS; ++c) {
                  if (keypad.was_pressed(r, c)) {
                      // Calculate a simple key number (0 to 39)
                      uint key_num = r * KEYPAD_COLS + c;
                      printf("Key Pressed: %u (Row: %u, Col: %u)\n", key_num, r, c);
                  }
                  // Optional: Check for release or hold
                  // if (keypad.was_released(r, c)) { ... }
                  // if (keypad.is_held(r, c)) { ... }
              }
          }
      }

      // Add a small delay to yield time
      sleep_ms(1);
  }
  
  return 0;
}
