#ifndef DRUM_PIZZA_HARDWARE_H
#define DRUM_PIZZA_HARDWARE_H

#include "musin/boards/dato_submarine.h"
#include <array>
#include <cstddef> // For size_t
#include <cstdint>

// Application-specific logical names for Mux, LEDs, etc.
// Physical pin definitions are now in musin/boards/dato_submarine.h

// The single ADC pin connected to the output of all multiplexers
constexpr uint32_t PIZZA_MUX_ADC_PIN = DATO_SUBMARINE_ADC_PIN;

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
const std::array<uint32_t, 4> analog_address_pins = {
    DATO_SUBMARINE_MUX_ADDR0_PIN, DATO_SUBMARINE_MUX_ADDR1_PIN, DATO_SUBMARINE_MUX_ADDR2_PIN,
    DATO_SUBMARINE_MUX_ADDR3_PIN};
// Static array for keypad column pins
const std::array<uint32_t, 5> keypad_columns_pins = {
    DATO_SUBMARINE_KEYPAD_COL1_PIN, DATO_SUBMARINE_KEYPAD_COL2_PIN, DATO_SUBMARINE_KEYPAD_COL3_PIN,
    DATO_SUBMARINE_KEYPAD_COL4_PIN, DATO_SUBMARINE_KEYPAD_COL5_PIN};
// Static array for keypad decoder address pins (uses first 3)
const std::array<uint32_t, 3> keypad_decoder_pins = {
    DATO_SUBMARINE_MUX_ADDR0_PIN, DATO_SUBMARINE_MUX_ADDR1_PIN, DATO_SUBMARINE_MUX_ADDR2_PIN};

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
