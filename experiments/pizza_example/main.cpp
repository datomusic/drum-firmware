/* TODO:
 - map button actions to correct leds
 - map slider position to drumpad brightness
 - sample select buttons select drumpad color
 - speed determines playbutton blue channel
 - tempo determines playbutton red channel

 */
#include <array>
#include <cstdint>
#include <cstdio>
#include <etl/array.h>
#include <iterator>
#include <optional> // For optional LED return pin
#include <cstddef> 

#include "pico/stdlib.h"
#include "pico/time.h"

#include "etl/span.h"
// analog_in.h is included by analog_control.h and drumpad.h
#include "musin/ui/analog_control.h"
#include "musin/ui/keypad_hc138.h"
#include "musin/drivers/ws2812.h"
#include "musin/ui/drumpad.h"       // Include the Drumpad driver

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

constexpr uint32_t PIN_LED_ENABLE = 20;
constexpr uint32_t PIN_LED_DATA = 16;

static constexpr std::uint32_t LED_PLAY_BUTTON = 0;
static constexpr std::uint32_t LED_STEP1_START = 1; // Includes LEDs 1, 2, 3, 4
static constexpr std::uint32_t LED_DRUMPAD_1   = 5;
static constexpr std::uint32_t LED_STEP2_START = 6; // Includes LEDs 6, 7, 8, 9
static constexpr std::uint32_t LED_STEP3_START = 10; // Includes LEDs 10, 11, 12, 13
static constexpr std::uint32_t LED_DRUMPAD_2   = 14;
static constexpr std::uint32_t LED_STEP4_START = 15; // Includes LEDs 15, 16, 17, 18
static constexpr std::uint32_t LED_STEP5_START = 19; // Includes LEDs 19, 20, 21, 22
static constexpr std::uint32_t LED_DRUMPAD_3   = 23;
static constexpr std::uint32_t LED_STEP6_START = 24; // Includes LEDs 24, 25, 26, 27
static constexpr std::uint32_t LED_STEP7_START = 28; // Includes LEDs 28, 29, 30, 31
static constexpr std::uint32_t LED_DRUMPAD_4   = 32;
static constexpr std::uint32_t LED_STEP8_START = 33; // Includes LEDs 33, 34, 35, 36

static constexpr std::uint32_t NUM_LEDS = 37;
Musin::Drivers::WS2812<NUM_LEDS> leds(PIN_LED_DATA,
Musin::Drivers::RGBOrder::GRB,
255, std::nullopt);

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

// --- Drumpad Configuration ---
// Define the multiplexer addresses for the drumpads
constexpr uint8_t DRUMPAD_ADDRESS_1 = 0;
constexpr uint8_t DRUMPAD_ADDRESS_2 = 2;
constexpr uint8_t DRUMPAD_ADDRESS_3 = 11;
constexpr uint8_t DRUMPAD_ADDRESS_4 = 13;

// Create AnalogInMux16 instances (readers) for each drumpad
// Using the alias from musin/hal/analog_in.h
static Musin::HAL::AnalogInMux16 reader_pad1(PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_1);
static Musin::HAL::AnalogInMux16 reader_pad2(PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_2);
static Musin::HAL::AnalogInMux16 reader_pad3(PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_3);
static Musin::HAL::AnalogInMux16 reader_pad4(PIN_ADC, analog_address_pins, DRUMPAD_ADDRESS_4);

// Create Drumpad instances using the specific readers
// Using default thresholds for now. Pass the reader by reference.
static Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> drumpad1(reader_pad1);
static Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> drumpad2(reader_pad2);
static Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> drumpad3(reader_pad3);
static Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> drumpad4(reader_pad4);

// Store drumpads in an array for easier iteration (using pointers)
static etl::array<Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>*, 4> drumpads = {
    &drumpad1, &drumpad2, &drumpad3, &drumpad4
};
// --- End Drumpad Configuration ---


// The actual MIDI sending function (prints and updates specific LEDs)
void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("MIDI CC %u: %u\n", cc_number, value);
}

void send_midi_note([[maybe_unused]] uint8_t channel, uint8_t note_number, uint8_t velocity) {
  printf("MIDI Note %u: %u\n", note_number, velocity);
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
  const MIDISendFn _send_midi_cc;

  // Constructor
  constexpr MIDICCObserver(uint8_t cc, uint8_t channel, MIDISendFn sender)
      : cc_number(cc), midi_channel(channel), _send_midi_cc(sender) {
  }

  void notification(Musin::UI::AnalogControlEvent event) override {
    // Convert normalized value (0.0-1.0) to MIDI CC value (0-127)
    uint8_t value = static_cast<uint8_t>(event.value * 127.0f);

    // Map specific CCs to specific LEDs
    uint32_t led_index_to_set = NUM_LEDS; // Initialize to an invalid index

    switch (cc_number) {
      case 21: // Speed
        led_index_to_set = LED_PLAY_BUTTON;
        // Maybe use value for blue channel? For now, just brightness.
        leds.set_pixel(led_index_to_set, value, value, value); // Example: Blue channel
        break;
      // case 16: // Drumpad 1 related?
      //   led_index_to_set = LED_DRUMPAD_1;
      //   leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
      //   send_midi_note(10, 36, value);
      //   break;
      case 18: // Drumpad 2 related?
        led_index_to_set = LED_DRUMPAD_2;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        break;
      case 27: // Drumpad 3 related?
        led_index_to_set = LED_DRUMPAD_3;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        break;
      case 29: // Drumpad 4 related?
        led_index_to_set = LED_DRUMPAD_4;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        break;
      // Add other CC mappings here if needed
      default:
        _send_midi_cc(midi_channel, cc_number, value);
        break;
    // Send MIDI CC message through function pointer
    }


  }
};

struct KeypadObserver : public etl::observer<Musin::UI::KeypadEvent> {
  const std::array<uint8_t, 40> &_cc_map; // Assuming 40 keys (8 rows x 5 cols)
  const uint8_t _midi_channel;

  using MIDISendFn = void (*)(uint8_t channel, uint8_t cc, uint8_t value);
  const MIDISendFn _send_midi_cc;

  // Constructor
  constexpr KeypadObserver(const std::array<uint8_t, 40> &map, uint8_t channel,
                                    MIDISendFn sender)
      : _cc_map(map), _midi_channel(channel), _send_midi_cc(sender) {
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
      _send_midi_cc(_midi_channel, cc_number, 100); // Send CC ON
      break;
    case Musin::UI::KeypadEvent::Type::Release:
      _send_midi_cc(_midi_channel, cc_number, 0); // Send CC OFF
      break;
    case Musin::UI::KeypadEvent::Type::Hold:
      _send_midi_cc(_midi_channel, cc_number, 127); // Send CC HOLD
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
    map[i] = (i) <= 119 ? (i) : 0; // Assign 0 if out of range
  }
  return map;
}();

static KeypadObserver keypad_map_observer(keypad_cc_map, 0, send_midi_cc);
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


  gpio_init(PIN_LED_ENABLE);
  gpio_set_dir(PIN_LED_ENABLE, true);
  gpio_put(PIN_LED_ENABLE, 1);

  sleep_ms(2000);
  
  leds.init();
  leds.set_pixel(0, 0x00ff00);
  leds.show();

  printf("MIDI CC and Keypad demo\n");

  // Initialize Keypad
  keypad.init();
  printf("Keypad Initialized (%u rows, %u cols)\n", KEYPAD_ROWS, KEYPAD_COLS);

  keypad.add_observer(keypad_map_observer);

  // Initialize Drumpad Readers
  reader_pad1.init();
  reader_pad2.init();
  reader_pad3.init();
  reader_pad4.init();
  printf("Drumpad Readers Initialized\n");
  // Drumpad objects themselves don't need init() as they use initialized readers

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

    // Update Drumpads and check for hits
    for (size_t i = 0; i < drumpads.size(); ++i) {
        if (drumpads[i]->update()) { // update() returns true if an update was performed
            if (drumpads[i]->was_pressed()) {
                auto velocity = drumpads[i]->get_velocity();
                if (velocity) {
                    printf("Drum %zu hit! Velocity: %u (Raw: %u, TimeDiff: %llu us)\n", // Add TimeDiff
                           i + 1, // Print 1-based index
                           *velocity,
                           drumpads[i]->get_raw_adc_value(),
                           drumpads[i]->get_last_velocity_time_diff()); // DEBUG: Print time diff
                    // TODO: Send MIDI note or trigger sound here
                }
            }
            // Optional: Check for release or hold events
            // if (drumpads[i]->was_released()) { printf("Drum %zu released\n", i + 1); }
            // if (drumpads[i]->is_held()) { printf("Drum %zu held\n", i + 1); }
        }
    }


    leds.show();
    // Add a small delay to yield time
    sleep_ms(1); // Consider if 1ms is frequent enough for drumpad scanning
  }

  return 0;
}
