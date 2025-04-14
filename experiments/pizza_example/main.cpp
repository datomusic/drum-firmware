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
#include <optional>
#include <cstddef> 

#include "pico/stdlib.h"
#include "pico/time.h"

#include "etl/span.h"
#include "musin/ui/analog_control.h"
#include "musin/ui/keypad_hc138.h"
#include "musin/drivers/ws2812.h"
#include "musin/ui/drumpad.h"

extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
}

#include "drum_pizza_pins.h"

using Musin::UI::AnalogControl;
using Musin::UI::Keypad_HC138;

Musin::Drivers::WS2812<NUM_LEDS> leds(PIN_LED_DATA,
Musin::Drivers::RGBOrder::GRB,
255, 0xffd0d0);

// Static array for multiplexer address pins (AnalogControls use 4)
const std::array<uint32_t, 4> analog_address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2,
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

void drumpads_update() {
  // Update Drumpads and check for hits
  for (size_t i = 0; i < drumpads.size(); ++i) {
    if (drumpads[i]->update()) {
      auto velocity = drumpads[i]->get_velocity();
      if (velocity) {
        printf("Drum %zu hit! Velocity: %u (Raw: %u, TimeDiff: %llu us)\n",
                i + 1,
                *velocity,
                drumpads[i]->get_raw_adc_value(),
                drumpads[i]->get_last_velocity_time_diff());
        // TODO: Send MIDI note or trigger sound here
      }
    }
  }
}

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
      case PLAYBUTTON: // Speed
        led_index_to_set = LED_PLAY_BUTTON;
        // Maybe use value for blue channel? For now, just brightness.
        leds.set_pixel(led_index_to_set, value, value, value); // Example: Blue channel
        break;
      case DRUM1:
        led_index_to_set = LED_DRUMPAD_1;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        send_midi_note(10, 36, value);
        break;
      case DRUM2: // Drumpad 2 related?
        led_index_to_set = LED_DRUMPAD_2;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        break;
      case DRUM3:
        led_index_to_set = LED_DRUMPAD_3;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        break;
      case DRUM4:
        led_index_to_set = LED_DRUMPAD_4;
        leds.set_pixel(led_index_to_set, value, value, value); // Grayscale brightness
        break;
      case SWING:
        printf("Swing set to %d\n", value);
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
    uint8_t key_index = (7-event.row) * 5 + event.col; // Assuming 5 columns
    if (key_index >= _cc_map.size())
      return;

    uint8_t cc_number = _cc_map[key_index];
    if (cc_number == 0)
      return; // Check if a valid CC was assigned

    uint8_t value = 0;

    switch (event.type) {
    case Musin::UI::KeypadEvent::Type::Press:
      value = 100;
      break;
    case Musin::UI::KeypadEvent::Type::Release:
      value = 0;
      break;
    case Musin::UI::KeypadEvent::Type::Hold:
      value = 127;
      break;
    }

    printf("Key %d %d\n", key_index, value);

    if (event.col == 4) {
      // Sample select buttons
      printf("Switch sample %d\n", (event.row));
    } else {
      leds.set_pixel(LED_ARRAY[(7-event.row) * 4 + event.col], value, value, value);
    }
  }
};

// Define the mapping from key index (0-39) to MIDI CC number
// Example: Starting at CC 32 and incrementing. Customize as needed.
constexpr std::array<uint8_t, KEYPAD_TOTAL_KEYS> keypad_cc_map = [] {
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
static etl::array<MIDICCObserver, 16> cc_observers = {{{ 0, 0, send_midi_cc},
                                                       { 1, 0, send_midi_cc},
                                                       { 2, 0, send_midi_cc},
                                                       { 3, 0, send_midi_cc},
                                                       { 4, 0, send_midi_cc},
                                                       { 5, 0, send_midi_cc},
                                                       { 6, 0, send_midi_cc},
                                                       { 7, 0, send_midi_cc},
                                                       { 8, 0, send_midi_cc},
                                                       { 9, 0, send_midi_cc},
                                                       {10, 0, send_midi_cc},
                                                       {11, 0, send_midi_cc},
                                                       {12, 0, send_midi_cc},
                                                       {13, 0, send_midi_cc},
                                                       {14, 0, send_midi_cc},
                                                       {15, 0, send_midi_cc}}};

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


enum class ExternalPinState {
  FLOATING,
  PULL_UP,
  PULL_DOWN,
  UNDETERMINED
};



static ExternalPinState check_external_pin_state(std::uint32_t gpio, const char* name) { // Use std::uint32_t
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
  const char* state_str;

  if (!initial_read && pullup_read && !pulldown_read) {
      // Reads LOW with no pull, HIGH with pull-up, LOW with pull-down -> Floating
      determined_state = ExternalPinState::FLOATING;
      state_str = "Floating";
  } else if (initial_read && pullup_read && !pulldown_read) {
       // Reads HIGH with no pull, HIGH with pull-up, LOW with pull-down -> Floating (alternative)
      determined_state = ExternalPinState::FLOATING;
      state_str = "Floating";
  } else if (!initial_read && !pullup_read) {
      // Reads LOW with no pull, LOW with pull-up -> Strong External Pull-down
      determined_state = ExternalPinState::PULL_DOWN;
      state_str = "External Pull-down";
  } else if (initial_read && pulldown_read) {
      // Reads HIGH with no pull, HIGH with pull-down -> Strong External Pull-up
      determined_state = ExternalPinState::PULL_UP;
      state_str = "External Pull-up";
  } else {
      // Other combinations are less clear or indicate potential issues
      determined_state = ExternalPinState::UNDETERMINED;
      state_str = "Undetermined / Inconsistent Reads";
  }

  printf("DrumPizza Init: Pin %lu (%s) external state check result: %s\n", gpio, name, state_str);

  gpio_disable_pulls(gpio);
  sleep_us(10);

  return determined_state;
}


void DrumPizza_init() {
  printf("DrumPizza Initializing...\n");

  printf("DrumPizza: Checking external pin states...\n");
  check_external_pin_state(PIN_ADDR_0, "ADDR_0");
  check_external_pin_state(PIN_ADDR_1, "ADDR_1");
  check_external_pin_state(PIN_ADDR_2, "ADDR_2");

  ExternalPinState led_pin_state = check_external_pin_state(PIN_LED_DATA, "LED_DATA");

  printf("DrumPizza: Initializing LEDs...\n");
  // Set brightness based on pin state check
  // If the pin is pulled up, assume SK6812, so 12mA per channel. Set brighness to 100
  // If it is pulled down, assume SK6805, so 5mA per channel. Set brightness to full
  uint8_t initial_brightness = (led_pin_state == ExternalPinState::PULL_UP) ? 100 : 255;
  printf("DrumPizza: Setting initial LED brightness to %u (based on pin state: %d)\n",
         initial_brightness, static_cast<int>(led_pin_state));
  leds.set_brightness(initial_brightness);

  if (!leds.init()) {
      printf("Error: Failed to initialize WS2812 LED driver!\n");
      // Depending on requirements, might panic here: panic("WS2812 Init Failed");
  } else {
      gpio_init(PIN_LED_ENABLE);
      gpio_set_dir(PIN_LED_ENABLE, true);
      gpio_put(PIN_LED_ENABLE, 1);

      leds.clear();
      leds.show();
  }

  printf("DrumPizza Initialization Complete.\n");
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf(".\n");
  sleep_ms(2000);
  DrumPizza_init();

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

    leds.show();

    drumpads_update();
    
    // Add a small delay to yield time
    sleep_us(80); // need at least 80us for the leds to latch
  }

  return 0;
}
