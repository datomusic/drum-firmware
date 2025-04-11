
#include <array>
#include <cstdint>
#include <cstdio>
#include <iterator>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "musin/ui/analog_control.h"
#include "musin/ui/keypad_hc138.h"
#include "etl/span.h"

extern "C" {
  #include "hardware/adc.h"
  #include "hardware/gpio.h"
}

using Musin::UI::AnalogControl;
using Musin::UI::Keypad_HC138;
using Musin::UI::KeyData;

constexpr auto PIN_ADDR_0 = 29;
constexpr auto PIN_ADDR_1 = 6;
constexpr auto PIN_ADDR_2 = 7;
constexpr auto PIN_ADDR_3 = 9;

constexpr auto PIN_ADC = 28;

constexpr auto PIN_RING_1 = 15;
constexpr auto PIN_RING_2 = 14;
constexpr auto PIN_RING_3 = 13;
constexpr auto PIN_RING_4 = 11;
constexpr auto PIN_RING_5 = 10;

// Static array for multiplexer address pins (AnalogControls use 4)
const std::array<std::uint32_t, 4> analog_address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2, PIN_ADDR_3};
// Static array for keypad column pins
const std::array<uint, 5> keypad_columns_pins = {PIN_RING_1, PIN_RING_2, PIN_RING_3, PIN_RING_4, PIN_RING_5};
// Static array for keypad decoder address pins (uses first 3)
const std::array<uint, 3> keypad_decoder_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2};

// --- Keypad Configuration ---
constexpr uint8_t KEYPAD_ROWS = 8; // Using 3 address pins allows up to 8 rows
constexpr uint8_t KEYPAD_COLS = std::size(keypad_columns_pins); // Based on keypad_columns_pins size
constexpr size_t KEYPAD_TOTAL_KEYS = KEYPAD_ROWS * KEYPAD_COLS;

// No longer need external buffer
// static std::array<KeyData, KEYPAD_TOTAL_KEYS> keypad_key_data_buffer;

// Static instance of the keypad driver
static Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS, 1> keypad( // Specify MaxObservers = 1 (or more if needed)
    keypad_decoder_pins,
    keypad_columns_pins,
    // No buffer argument needed
    10'000U, // 10ms scan time
    5'000U,  //  5ms debounce time
    1'000'000U // 1 second hold time
);
// --- End Keypad Configuration ---

// Define the function pointer type needed by observers
using MIDISendFn = void (*)(uint8_t channel, uint8_t cc, uint8_t value);

// The actual MIDI sending function (currently prints)
void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("MIDI CC %u: %u\n", cc_number, value);
}

// --- Keypad MIDI Map Observer Implementation ---
#include "observers.h"

// Define the mapping from key index (0-39) to MIDI CC number
// Example: Starting at CC 32 and incrementing. Customize as needed.
static constexpr std::array<uint8_t, KEYPAD_TOTAL_KEYS> keypad_cc_map = [] {
    std::array<uint8_t, KEYPAD_TOTAL_KEYS> map{};
    for (size_t i = 0; i < KEYPAD_TOTAL_KEYS; ++i) {
        // Ensure CC stays within 0-119 range
        map[i] = (32 + i) <= 119 ? (32 + i) : 0; // Assign 0 if out of range
    }
    return map;
}();

static KeypadMIDICCMapObserver keypad_map_observer(keypad_cc_map, 0, send_midi_cc);
// --- End Keypad MIDI Map Observer ---

// Define MIDI observers statically
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
static AnalogControl<1> mux_controls[] = { // Unique ID, ADC pin, address pins, address
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
  stdio_init_all();
  
  sleep_ms(1000);

  printf("MIDI CC and Keypad demo\n");

  // Initialize Keypad
  keypad.init();
  printf("Keypad Initialized (%u rows, %u cols)\n", KEYPAD_ROWS, KEYPAD_COLS);
  
  if (!keypad.add_observer(&keypad_map_observer)) {
      printf("Error: Could not add keypad map observer!\n");
  }

  // Ensure control and observer arrays have the same size before iterating
  static_assert(std::size(mux_controls) == std::size(cc_observers), 
                "Mismatch between number of controls and observers");

  // Initialize Analog Controls using std::size to determine the loop bounds
  for (size_t i = 0; i < std::size(mux_controls); ++i) {
    mux_controls[i].init();
    if (!mux_controls[i].add_observer(&cc_observers[i])) {
        printf("Error: Could not add observer for analog control %zu (ID: %u)\n", i, mux_controls[i].get_id());
        // Handle error appropriately if needed
    }
  }

  printf("Initialized %zu analog controls\n", std::size(mux_controls));

  while (true) {
      // Update all analog mux controls - observers will be notified automatically
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
