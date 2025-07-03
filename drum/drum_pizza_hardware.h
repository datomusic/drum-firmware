#ifndef DRUM_PIZZA_HARDWARE_H
#define DRUM_PIZZA_HARDWARE_H

#include "musin/boards/dato_submarine.h"
#include "musin/hal/logger.h"
#include "etl/array.h"
#include <cstddef> // For size_t
#include <cstdint>
#include <cstdio>

extern "C" {
#include "hardware/gpio.h"
#include "pico/time.h"
}

// Application-specific logical names for Mux, LEDs, etc.
// Physical pin definitions are now in musin/boards/dato_submarine.h

// The single ADC pin connected to the output of all multiplexers
constexpr uint32_t PIZZA_MUX_ADC_PIN = DATO_SUBMARINE_ADC_PIN;
static_assert(PIZZA_MUX_ADC_PIN >= 26 && PIZZA_MUX_ADC_PIN <= 29,
              "PIZZA_MUX_ADC_PIN must be a valid ADC pin (26-29)");

// Logical names for LED driver pins
constexpr uint32_t PIZZA_LED_DATA_PIN = PICO_DEFAULT_WS2812_PIN;
constexpr uint32_t PIZZA_LED_ENABLE_PIN = DATO_SUBMARINE_LED_ENABLE_PIN;

constexpr uint32_t LED_PLAY_BUTTON = 0;

constexpr uint32_t LED_DRUMPAD_1 = 5;
constexpr uint32_t LED_DRUMPAD_2 = 14;
constexpr uint32_t LED_DRUMPAD_3 = 23;
constexpr uint32_t LED_DRUMPAD_4 = 32;

constexpr uint32_t LED_STEP1_START = 1;
constexpr uint32_t LED_STEP2_START = 6;
constexpr uint32_t LED_STEP3_START = 10;
constexpr uint32_t LED_STEP4_START = 15;
constexpr uint32_t LED_STEP5_START = 19;
constexpr uint32_t LED_STEP6_START = 24;
constexpr uint32_t LED_STEP7_START = 28;
constexpr uint32_t LED_STEP8_START = 33;

// LED indices for the 8x4 sequencer grid
constexpr etl::array<uint32_t, 32> LED_ARRAY = {
    1,  2,  3,  4,  // Step 1
    6,  7,  8,  9,  // Step 2
    10, 11, 12, 13, // Step 3
    15, 16, 17, 18, // Step 4
    19, 20, 21, 22, // Step 5
    24, 25, 26, 27, // Step 6
    28, 29, 30, 31, // Step 7
    33, 34, 35, 36  // Step 8
};

// Total number of LEDs: 32 sequencer steps + 4 drumpads + 1 play button
constexpr uint32_t NUM_LEDS = LED_ARRAY.size() + 4 + 1;

// Mux addresses for analog inputs (Control IDs)
enum {
  DRUM1 = 0,
  FILTER = 1,
  DRUM2 = 2,
  PITCH1 = 3,
  PITCH2 = 4,
  PLAYBUTTON = 5,
  RANDOM = 6,
  VOLUME = 7,
  PITCH3 = 8,
  SWING = 9,
  CRUSH = 10,
  DRUM3 = 11,
  REPEAT = 12,
  DRUM4 = 13,
  SPEED = 14,
  PITCH4 = 15
};

// Static array for multiplexer address pins (AnalogControls use 4)
const etl::array<uint32_t, 4> analog_address_pins = {
    DATO_SUBMARINE_MUX_ADDR0_PIN, DATO_SUBMARINE_MUX_ADDR1_PIN, DATO_SUBMARINE_MUX_ADDR2_PIN,
    DATO_SUBMARINE_MUX_ADDR3_PIN};
// Static array for keypad column pins
const etl::array<uint32_t, 5> keypad_columns_pins = {
    DATO_SUBMARINE_KEYPAD_COL1_PIN, DATO_SUBMARINE_KEYPAD_COL2_PIN, DATO_SUBMARINE_KEYPAD_COL3_PIN,
    DATO_SUBMARINE_KEYPAD_COL4_PIN, DATO_SUBMARINE_KEYPAD_COL5_PIN};
// Static array for keypad decoder address pins (uses first 3)
const etl::array<uint32_t, 3> keypad_decoder_pins = {
    DATO_SUBMARINE_MUX_ADDR0_PIN, DATO_SUBMARINE_MUX_ADDR1_PIN, DATO_SUBMARINE_MUX_ADDR2_PIN};

// --- Keypad Configuration ---
constexpr uint8_t KEYPAD_ROWS = 8;
constexpr uint8_t KEYPAD_COLS = std::size(keypad_columns_pins);
constexpr size_t KEYPAD_TOTAL_KEYS = KEYPAD_ROWS * KEYPAD_COLS;


constexpr etl::array<uint8_t, 4> drumpad_addresses = {
  0, 2, 11, 13
};

// --- Hardware Utilities ---

constexpr auto PULL_CHECK_DELAY_US = 1000;

enum class ExternalPinState {
  FLOATING,
  PULL_UP,
  PULL_DOWN,
  UNDETERMINED
};

/**
 * @brief Checks the external pull-up/pull-down state of a GPIO pin.
 *
 * This function temporarily configures a GPIO pin to determine if it is
 * floating, pulled up, or pulled down externally. It restores the pin's
 * pull state to disabled before returning.
 *
 * @param gpio The GPIO pin number to check.
 * @param logger A logger instance for debug output.
 * @return The determined state of the pin (FLOATING, PULL_UP, PULL_DOWN, or UNDETERMINED).
 */
inline ExternalPinState check_external_pin_state(std::uint32_t gpio, musin::Logger &logger) {
  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);

  gpio_disable_pulls(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool initial_read = gpio_get(gpio);

  gpio_pull_up(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool pullup_read = gpio_get(gpio);

  ExternalPinState determined_state;

  // The logic for determining the state is based on how the pin behaves with an internal pull-up.
  // This avoids using the internal pull-down, which is buggy on the RP2350.
  // - A floating pin will read LOW without pull and HIGH with pull-up.
  // - A pin with an external pull-up will read HIGH in both cases.
  // - A pin with an external pull-down will read LOW in both cases.
  if (!initial_read && pullup_read) {
    determined_state = ExternalPinState::FLOATING;
  } else if (initial_read && pullup_read) {
    determined_state = ExternalPinState::PULL_UP;
  } else if (!initial_read && !pullup_read) {
    determined_state = ExternalPinState::PULL_DOWN;
  } else {
    determined_state = ExternalPinState::UNDETERMINED;
  }

  const char *state_str;
  switch (determined_state) {
  case ExternalPinState::FLOATING:
    state_str = "FLOATING";
    break;
  case ExternalPinState::PULL_UP:
    state_str = "PULL_UP";
    break;
  case ExternalPinState::PULL_DOWN:
    state_str = "PULL_DOWN";
    break;
  default:
    state_str = "UNDETERMINED";
    break;
  }

  char buffer[128];
  snprintf(buffer, sizeof(buffer), "Pin check GPIO %2lu -> %-12s (initial=%d, pullup=%d)",
           static_cast<unsigned long>(gpio), state_str, initial_read, pullup_read);
  logger.debug(buffer);

  gpio_disable_pulls(gpio);
  sleep_us(PULL_CHECK_DELAY_US);

  return determined_state;
}

/**
 * @brief Checks if the control panel is disconnected by checking for floating pins.
 *
 * This is used to detect if the control panel is not properly connected. If all
 * of the first three analog multiplexer address pins are floating, it's assumed
 * the panel is absent or faulty, and local control should be disabled.
 *
 * @param logger A logger instance for debug output.
 * @return true if all three checked pins are floating, false otherwise.
 */
inline bool is_control_panel_disconnected(musin::Logger &logger) {
  // We check the first 3 address pins. If all are floating, we assume the panel is disconnected.
  // The 4th pin is not checked as it lacks external pull resistors.
  if (check_external_pin_state(analog_address_pins[0], logger) != ExternalPinState::FLOATING) {
    return false;
  }
  if (check_external_pin_state(analog_address_pins[1], logger) != ExternalPinState::FLOATING) {
    return false;
  }
  if (check_external_pin_state(analog_address_pins[2], logger) != ExternalPinState::FLOATING) {
    return false;
  }
  return true; // All three are floating
}

#endif // DRUM_PIZZA_HARDWARE_H
