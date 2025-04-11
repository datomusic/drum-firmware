// static_example_main.cpp
#include "pico/stdlib.h"
#include <cstdint>
#include <cstdio>
#include "pico/time.h"
#include <array>
#include <iterator> // For std::size

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
static Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS, 1> keypad( // Specify MaxObservers = 1 (or more if needed)
    keypad_decoder_pins,
    keypad_columns_pins,
    etl::span<KeyData>(keypad_key_data_buffer) // Create span from the buffer
);
// --- End Keypad Configuration ---


void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("MIDI CC %u: %u\n", cc_number, value);
}

// --- Keypad MIDI Observer Implementation ---
struct KeypadMIDICC : public KeypadObserverBase {
    // Starting CC number for keypad keys
    static constexpr uint8_t START_CC = 32;
    // MIDI channel to send on
    static constexpr uint8_t MIDI_CHANNEL = 0;

    void on_key_pressed(uint8_t row, uint8_t col) override {
        uint8_t key_index = row * KEYPAD_COLS + col;
        uint8_t cc_number = START_CC + key_index;
        // Ensure CC number doesn't exceed 119 (standard range)
        if (cc_number <= 119) {
            send_midi_cc(MIDI_CHANNEL, cc_number, 127); // Send CC ON
        }
    }

    void on_key_released(uint8_t row, uint8_t col) override {
        uint8_t key_index = row * KEYPAD_COLS + col;
        uint8_t cc_number = START_CC + key_index;
        // Ensure CC number doesn't exceed 119 (standard range)
        if (cc_number <= 119) {
            send_midi_cc(MIDI_CHANNEL, cc_number, 0); // Send CC OFF
        }
    }

    // We don't need to do anything on hold for this example
    void on_key_held(uint8_t row, uint8_t col) override {
        // No action needed
    }
};

// Static instance of the keypad observer
static KeypadMIDICC keypad_observer;
// --- End Keypad MIDI Observer ---

// Define MIDI observers statically
// These will be allocated at compile time
static MIDICCObserver cc_observers[] = {
  {16, 0, send_midi_cc},
  {17, 0, send_midi_cc},
  {18, 0, send_midi_cc},
  {19, 0, send_midi_cc},
  {20, 0, send_midi_cc},
  {21, 0, send_midi_cc},
  {22, 0, send_midi_cc},
  {23, 0, send_midi_cc},
  {24, 0, send_midi_cc},
  {25, 0, send_midi_cc},
  {26, 0, send_midi_cc},
  {27, 0, send_midi_cc},
  {28, 0, send_midi_cc},
  {29, 0, send_midi_cc},
  {30, 0, send_midi_cc},
  {31, 0, send_midi_cc}
};

// Statically allocate multiplexed controls using the class from musin::ui
static AnalogControl<1> mux_controls[] = {
  {10, PIN_ADC, analog_address_pins, 0 },
  {11, PIN_ADC, analog_address_pins, 1 },
  {12, PIN_ADC, analog_address_pins, 2 },
  {13, PIN_ADC, analog_address_pins, 3 },
  {14, PIN_ADC, analog_address_pins, 4 },
  {15, PIN_ADC, analog_address_pins, 5 },
  {16, PIN_ADC, analog_address_pins, 6 },
  {17, PIN_ADC, analog_address_pins, 7 },
  {18, PIN_ADC, analog_address_pins, 8 },
  {19, PIN_ADC, analog_address_pins, 9 },
  {20, PIN_ADC, analog_address_pins, 10 },
  {21, PIN_ADC, analog_address_pins, 11 },
  {22, PIN_ADC, analog_address_pins, 12 },
  {23, PIN_ADC, analog_address_pins, 13 },
  {24, PIN_ADC, analog_address_pins, 14 },
  {25, PIN_ADC, analog_address_pins, 15 }
};


int main() {
  // Initialize system
  stdio_init_all();
  
  sleep_ms(1000);

  printf("MIDI CC and Keypad demo\n");

  // Initialize Keypad
  keypad.init();
  printf("Keypad Initialized (%u rows, %u cols)\n", KEYPAD_ROWS, KEYPAD_COLS);
  // Register the observer
  if (!keypad.add_observer(&keypad_observer)) {
      printf("Error: Could not add keypad observer!\n");
      // Handle error appropriately if needed
  }

  // Ensure control and observer arrays have the same size before iterating
  static_assert(std::size(mux_controls) == std::size(cc_observers), 
                "Mismatch between number of controls and observers");

  // Initialize Analog Controls using std::size to determine the loop bounds
  for (size_t i = 0; i < std::size(mux_controls); ++i) {
    mux_controls[i].init();
    mux_controls[i].add_observer(&cc_observers[i]);
  }

  printf("Initialized %zu analog controls\n", std::size(mux_controls));

  while (true) {
      // Update all mux controls
      for (auto& control : mux_controls) {
          control.update();
      }

      // Scan the keypad - observers will be notified automatically
      keypad.scan();

      // Add a small delay to yield time
      sleep_ms(1);
  }
  
  return 0;
}
