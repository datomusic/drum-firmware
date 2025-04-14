
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

// --- Keypad MIDI Map Observer Implementation ---
/**
 * @brief MIDI CC observer implementation
 * Statically configured, no dynamic memory allocation.
 * This remains specific to the experiment.
 */
struct MIDICCObserver : public etl::observer<Musin::UI::AnalogControlEvent> {
  const uint8_t cc_number;
  const uint8_t midi_channel;

  using MIDISendFn = void (*)(uint8_t channel, uint8_t cc, uint8_t value);
  const MIDISendFn _send_midi;

  // Constructor
  constexpr MIDICCObserver(uint8_t cc, uint8_t channel, MIDISendFn sender)
      : cc_number(cc), midi_channel(channel), _send_midi(sender) {
  }

  void notification(Musin::UI::AnalogControlEvent event) override {
    // Convert normalized value (0.0-1.0) to MIDI CC value (0-127)
    uint8_t cc_value = static_cast<uint8_t>(event.value * 127.0f);

    // Send MIDI CC message through function pointer
    _send_midi(midi_channel, cc_number, cc_value);
  }
};

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

// Define MIDI observers statically
static etl::array<MIDICCObserver, 16> cc_observers = {{{16, 0, send_midi_cc},
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
                                                       {31, 0, send_midi_cc}}};

// Statically allocate multiplexed controls using the class from musin::ui
static etl::array<AnalogControl, 16> mux_controls = {{{10, PIN_ADC, analog_address_pins, 0},
                                                      {11, PIN_ADC, analog_address_pins, 1},
                                                      {12, PIN_ADC, analog_address_pins, 2},
                                                      {13, PIN_ADC, analog_address_pins, 3},
                                                      {14, PIN_ADC, analog_address_pins, 4},
                                                      {15, PIN_ADC, analog_address_pins, 5},
                                                      {16, PIN_ADC, analog_address_pins, 6},
                                                      {17, PIN_ADC, analog_address_pins, 7},
                                                      {18, PIN_ADC, analog_address_pins, 8},
                                                      {19, PIN_ADC, analog_address_pins, 9},
                                                      {20, PIN_ADC, analog_address_pins, 10},
                                                      {21, PIN_ADC, analog_address_pins, 11},
                                                      {22, PIN_ADC, analog_address_pins, 12},
                                                      {23, PIN_ADC, analog_address_pins, 13},
                                                      {24, PIN_ADC, analog_address_pins, 14},
                                                      {25, PIN_ADC, analog_address_pins, 15}}};

int main() {
  stdio_init_all();

  sleep_ms(1000);

  printf("MIDI CC and Keypad demo\n");

  // Initialize Keypad
  keypad.init();
  printf("Keypad Initialized (%u rows, %u cols)\n", KEYPAD_ROWS, KEYPAD_COLS);

  keypad.add_observer(keypad_map_observer);

  // Initialize Analog Controls using index loop
  for (size_t i = 0; i < mux_controls.size(); ++i) {
    mux_controls[i].init();
    mux_controls[i].add_observer(cc_observers[i]);
  }

  printf("Initialized %zu analog controls\n", std::size(mux_controls));

  while (true) {
    // Update all analog mux controls - observers will be notified automatically
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
