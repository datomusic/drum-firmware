/*
 * Custom board definition for DATO Pizza
 */
#ifndef MUSIN_BOARDS_DATO_PIZZA_H
#define MUSIN_BOARDS_DATO_PIZZA_H

#include <cstdint>
#include <array>
#include <cstddef> // For size_t

// No direct SDK includes here, as this file defines the board interface,
// not the microcontroller mapping.

namespace Musin::Boards {

/**
 * @brief Represents the DRUM-Pizza 0.1 control board hardware interface.
 *
 * This header provides constants, named connector pins, and configurations
 * based on the DRUM-Pizza 0.1 specification and its J1 connector.
 * It defines the *interface* of the board, independent of the specific
 * microcontroller GPIOs it might be connected to. A separate mapping layer
 * is needed to translate these named pins to actual GPIO numbers.
 */
class DrumPizza {
public:
    /**
     * @brief Initialize the board specific components (if any).
     * Currently empty, placeholder for future use.
     */
    void init();

    // --- J1 Connector Pin Definitions ---
    // Represents the pins on the 20-pin J1 connector.
// Ground and Power pins are included for completeness but might not be
// directly used in software pin mapping logic. NC pins are also listed.
enum class J1PinName : std::uint8_t {
    PIN_1_MUX_IO,
    PIN_2_GND,
    PIN_3_P3V3A,
    PIN_4_GND,
    PIN_5_ADDR_0,
    PIN_6_ADDR_1,
    PIN_7_ADDR_2,
    PIN_8_NC,
    PIN_9_GND,
    PIN_10_ADDR_3,
    PIN_11_RING5,
    PIN_12_RING4,
    PIN_13_GND,
    PIN_14_RING3,
    PIN_15_RING2,
    PIN_16_RING1,
    PIN_17_NC,
    PIN_18_LED_DATA_RETURN,
    PIN_19_LED_DATA,
    PIN_20_VLED,
    _COUNT // Helper to get the number of enum values if needed
};

// --- General Configuration ---
// Board revision detection pins (if needed by software) - Uses J1 names
// static constexpr J1PinName PIN_BOARD_ADDR0 = J1PinName::PIN_5_ADDR_0; // Example
// static constexpr J1PinName PIN_BOARD_ADDR1 = J1PinName::PIN_6_ADDR_1; // Example
// static constexpr J1PinName PIN_BOARD_ADDR2 = J1PinName::PIN_7_ADDR_2; // Example

// --- Keypad Configuration ---
static constexpr std::uint8_t KEYPAD_ROWS = 8;
static constexpr std::uint8_t KEYPAD_COLS = 5;

// Keypad uses decoder address lines and column lines from J1
static constexpr std::array<J1PinName, 3> keypad_decoder_address_pins = {
    J1PinName::PIN_5_ADDR_0, // Decoder A0
    J1PinName::PIN_6_ADDR_1, // Decoder A1
    J1PinName::PIN_7_ADDR_2  // Decoder A2
};

static constexpr J1PinName keypad_column_pins[KEYPAD_COLS] = {
    J1PinName::PIN_16_RING1, // Column 0
    J1PinName::PIN_15_RING2, // Column 1
    J1PinName::PIN_14_RING3, // Column 2
    J1PinName::PIN_12_RING4, // Column 3
    J1PinName::PIN_11_RING5  // Column 4
};

// Symbolic names for columns, matching the array index above
enum class KeypadColumn : std::uint8_t {
    RING1 = 0, // Drum 1 Sequencer
    RING2 = 1, // Drum 2 Sequencer
    RING3 = 2, // Drum 3 Sequencer
    RING4 = 3, // Drum 4 Sequencer
    RING5 = 4  // Sample Select
};

// --- Analog Input Configuration ---
static constexpr std::uint8_t NUM_ANALOG_INPUTS = 16;

// Analog MUX uses address lines (shared with keypad) and a select line from J1
static constexpr J1PinName PIN_MUX_IO       = J1PinName::PIN_1_MUX_IO;     // Shared MUX I/O
static constexpr J1PinName PIN_MUX_SELECT   = J1PinName::PIN_10_ADDR_3;    // MUX selection (ADDR3)
// MUX Address pins (A0, A1, A2) are shared: keypad_decoder_address_pins

// Enum mapping MUX address (0-15) to control function. This is independent of GPIOs.
enum class AnalogInput : std::uint8_t {
    PITCH3     = 0,
    SWING      = 1,
    CRUSH      = 2,
    DRUM3      = 3,
    REPEAT     = 4,
    DRUM4      = 5,
    SPEED      = 6,
    PITCH4     = 7,
    DRUM1      = 8,
    FILTER     = 9,
    DRUM2      = 10,
    PITCH1     = 11,
    PITCH2     = 12,
    PLAYBUTTON = 13,
    RANDOM     = 14,
    VOLUME     = 15
};

// --- LED Output Configuration ---
static constexpr std::uint32_t NUM_LEDS = 37;

// LED strip data line from J1
static constexpr J1PinName PIN_LED_DATA = J1PinName::PIN_19_LED_DATA;
// static constexpr J1PinName PIN_LED_DATA_RETURN = J1PinName::PIN_18_LED_DATA_RETURN; // Optional

// Symbolic indices for specific LEDs (0-indexed)
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
// Helper function to get the 4 LEDs for a given step (1-indexed)
static constexpr std::array<std::uint32_t, 4> get_step_leds(std::uint8_t step_index_1_based) {
    // Note: This calculation assumes the LED indexing pattern holds.
    // step_index_1_based: 1 -> 1, 2, 3, 4
    // step_index_1_based: 2 -> 6, 7, 8, 9
    // step_index_1_based: 3 -> 10, 11, 12, 13
    // ...
    // Formula: start_index = (step_index_1_based <= 1 ? 1 : (step_index_1_based - 1) * 4 + (step_index_1_based <= 1 ? 0 : 2))
    // Let's use a simpler lookup based on the defined start indices
    uint32_t start_index = 0; // Needs to be non-const for the switch
    switch(step_index_1_based) {
        case 1: start_index = LED_STEP1_START; break;
        case 2: start_index = LED_STEP2_START; break;
        case 3: start_index = LED_STEP3_START; break;
        case 4: start_index = LED_STEP4_START; break;
        case 5: start_index = LED_STEP5_START; break;
        case 6: start_index = LED_STEP6_START; break;
        case 7: start_index = LED_STEP7_START; break;
        case 8: start_index = LED_STEP8_START; break;
        default: // Should not happen for 8 steps
                 // Return an invalid array or handle error appropriately
                 // Consider adding an assertion or returning std::optional
                 return {0, 0, 0, 0}; // Example: return invalid indices
        }
         // Ensure step_index is valid (1-8) before calling this or add checks
        return {start_index, start_index + 1, start_index + 2, start_index + 3};
    }

// Private members if needed in the future
// private:

}; // class DrumPizza

} // namespace Musin::Boards

#endif // MUSIN_BOARDS_DATO_PIZZA_H
