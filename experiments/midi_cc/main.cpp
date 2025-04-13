
#include <array>
#include <cstdint>
#include <cstdio>
#include <etl/array.h>
#include <iterator>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "etl/span.h"
#include "musin/ui/analog_control.h"
#include "musin/ui/keypad_hc138.h"

extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
}

using Musin::UI::AnalogControl;
using Musin::UI::Keypad_HC138;

constexpr uint32_t PIN_ADDR_0 = 29;
constexpr uint32_t PIN_ADDR_1 = 6;
constexpr uint32_t PIN_ADDR_2 = 7;
constexpr uint32_t PIN_ADDR_3 = 9;

constexpr uint32_t PIN_ADC = 28;

constexpr uint32_t PIN_RING_1 = 15;
constexpr uint32_t PIN_RING_2 = 14;
constexpr uint32_t PIN_RING_3 = 13;
constexpr uint32_t PIN_RING_4 = 11;
constexpr uint32_t PIN_RING_5 = 10;

// Static array for multiplexer address pins (AnalogControls use 4)
const std::array<std::uint32_t, 4> analog_address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2,
                                                          PIN_ADDR_3};
// Static array for keypad column pins
const std::array<uint, 5> keypad_columns_pins = {PIN_RING_1, PIN_RING_2, PIN_RING_3, PIN_RING_4,
                                                 PIN_RING_5};
// Static array for keypad decoder address pins (uses first 3)
const std::array<uint, 3> keypad_decoder_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2};

// --- Keypad Configuration ---
constexpr uint8_t KEYPAD_ROWS = 8; // Using 3 address pins allows up to 8 rows
constexpr uint8_t KEYPAD_COLS = std::size(keypad_columns_pins);
constexpr size_t KEYPAD_TOTAL_KEYS = KEYPAD_ROWS * KEYPAD_COLS;

// Static instance of the keypad driver
static Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad(keypad_decoder_pins, keypad_columns_pins,
                                                     10,  // 10ms scan time
                                                     5,   //  5ms debounce time
                                                     1000 // 1 second hold time
);

// The actual MIDI sending function (currently prints)
void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("MIDI CC %u: %u\n", cc_number, value);
}

// --- Analog Control Callback ---
// Callback function for AnalogControl events
void handle_analog_control_event(const Musin::UI::AnalogControlEvent& event) {
    // Extract the mux channel from the upper byte of source_id
    // Assumes source_id was created with (channel << 8) | pin
    uint8_t mux_channel = (event.source_id >> 8) & 0xFF;

    // Map mux channel (0-15) directly to MIDI CC (16-31)
    // Add checks if needed for different ADC pins or non-mux controls
    if (mux_channel <= 15) { // Ensure it's within the expected range for this setup
        uint8_t cc_number = mux_channel + 16;

        // Convert normalized value (0.0-1.0) to MIDI CC value (0-127)
        uint8_t cc_value = static_cast<uint8_t>(event.value * 127.0f);

        // Assuming MIDI channel 0 for this example
        send_midi_cc(0, cc_number, cc_value);
    }
    // else { handle other source_ids if necessary }
}
// --- End Analog Control Callback ---


// --- Keypad MIDI Map Observer Implementation ---
struct KeypadMIDICCMapObserver : public etl::observer<Musin::UI::KeypadEvent> {
  const std::array<uint8_t, 40> &_cc_map; // Assuming 40 keys (8 rows x 5 cols)
  const uint8_t _midi_channel;

  using MIDISendFn = void (*)(uint8_t channel, uint8_t cc, uint8_t value);
  const MIDISendFn _send_midi;

  // Constructor
  constexpr KeypadMIDICCMapObserver(const std::array<uint8_t, 40> &map, uint8_t channel,
                                    MIDISendFn sender)
      : _cc_map(map), _midi_channel(channel), _send_midi(sender) {
  }

  void notification(Musin::UI::KeypadEvent event) override {
    uint8_t key_index = event.row * 5 + event.col; // Assuming 5 columns
    if (key_index >= _cc_map.size())
      return;

    uint8_t cc_number = _cc_map[key_index];
    if (cc_number == 0)
      return; // Check if a valid CC was assigned

    switch (event.type) {
    case Musin::UI::KeypadEvent::Type::Press:
      _send_midi(_midi_channel, cc_number, 100); // Send CC ON
      break;
    case Musin::UI::KeypadEvent::Type::Release:
      _send_midi(_midi_channel, cc_number, 0); // Send CC OFF
      break;
    case Musin::UI::KeypadEvent::Type::Hold:
      _send_midi(_midi_channel, cc_number, 127); // Send CC HOLD
      break;
    }
  }
};

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


// Statically allocate multiplexed controls using the class from musin::ui
// Pass the callback function as a template argument
// The type is Musin::UI::AnalogControl<handle_analog_control_event>
// Note: The ID parameter is removed from the initializers
static etl::array<Musin::UI::AnalogControl<handle_analog_control_event>, 16> mux_controls = {{
  {PIN_ADC, analog_address_pins, 0},  // Mux Channel 0
  {PIN_ADC, analog_address_pins, 1},  // Mux Channel 1
  {PIN_ADC, analog_address_pins, 2},  // Mux Channel 2
  {PIN_ADC, analog_address_pins, 3},  // Mux Channel 3
  {PIN_ADC, analog_address_pins, 4},  // Mux Channel 4
  {PIN_ADC, analog_address_pins, 5},  // Mux Channel 5
  {PIN_ADC, analog_address_pins, 6},  // Mux Channel 6
  {PIN_ADC, analog_address_pins, 7},  // Mux Channel 7
  {PIN_ADC, analog_address_pins, 8},  // Mux Channel 8
  {PIN_ADC, analog_address_pins, 9},  // Mux Channel 9
  {PIN_ADC, analog_address_pins, 10}, // Mux Channel 10
  {PIN_ADC, analog_address_pins, 11}, // Mux Channel 11
  {PIN_ADC, analog_address_pins, 12}, // Mux Channel 12
  {PIN_ADC, analog_address_pins, 13}, // Mux Channel 13
  {PIN_ADC, analog_address_pins, 14}, // Mux Channel 14
  {PIN_ADC, analog_address_pins, 15}}}; // Mux Channel 15

int main() {
  stdio_init_all();

  sleep_ms(1000);

  printf("MIDI CC and Keypad demo\n");

  // Initialize Keypad
  keypad.init();
  printf("Keypad Initialized (%u rows, %u cols)\n", KEYPAD_ROWS, KEYPAD_COLS);

  keypad.add_observer(keypad_map_observer);

  // Initialize Analog Controls using range-based for loop
  for (auto& control : mux_controls) {
      control.init();
  }

  printf("Initialized %zu analog controls\n", mux_controls.size());

  while (true) {
    // Update all analog mux controls - callback will be invoked automatically
    for (auto &control : mux_controls) {
      control.update();
    }

    // Scan the keypad - observers will be notified automatically
    keypad.scan();

    // Add a small delay to yield time
    sleep_ms(1);
  }

  return 0;
}
