#ifndef DRUM_PIZZA_HARDWARE_H
#define DRUM_PIZZA_HARDWARE_H

#include <array>
#include <cstddef> // For size_t
#include <cstdint>

// Address pins for the multiplexer and decoder
constexpr uint32_t PIN_ADDR_0 = 6;
constexpr uint32_t PIN_ADDR_1 = 7;
constexpr uint32_t PIN_ADDR_2 = 8;
constexpr uint32_t PIN_ADDR_3 = 9;

// Common pin for multiplexer output
constexpr uint32_t PIN_ADC = 26;

// RING_1 through RING_4 are the sequencer track buttons
constexpr uint32_t PIN_RING_1 = 14;
constexpr uint32_t PIN_RING_2 = 13;
constexpr uint32_t PIN_RING_3 = 12;
constexpr uint32_t PIN_RING_4 = 11;
// RING_5 holds the sample switch buttons
constexpr uint32_t PIN_RING_5 = 10;

constexpr uint32_t PIN_LED_ENABLE = 17;
constexpr uint32_t PIN_LED_DATA = 16;

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
constexpr std::array<uint32_t, 32> LED_ARRAY = {
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
const std::array<uint32_t, 4> analog_address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2,
                                                     PIN_ADDR_3};
// Static array for keypad column pins
const std::array<uint32_t, 5> keypad_columns_pins = {PIN_RING_1, PIN_RING_2, PIN_RING_3, PIN_RING_4,
                                                     PIN_RING_5};
// Static array for keypad decoder address pins (uses first 3)
const std::array<uint32_t, 3> keypad_decoder_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2};

// --- Keypad Configuration ---
constexpr uint8_t KEYPAD_ROWS = 8;
constexpr uint8_t KEYPAD_COLS = std::size(keypad_columns_pins);
constexpr size_t KEYPAD_TOTAL_KEYS = KEYPAD_ROWS * KEYPAD_COLS;

// --- Drumpad Configuration ---
constexpr uint8_t DRUMPAD_ADDRESS_1 = 0;
constexpr uint8_t DRUMPAD_ADDRESS_2 = 2;
constexpr uint8_t DRUMPAD_ADDRESS_3 = 11;
constexpr uint8_t DRUMPAD_ADDRESS_4 = 13;

#endif // DRUM_PIZZA_HARDWARE_H
