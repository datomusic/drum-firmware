/*
 * Custom board definition for DATO Pizza
 */
#ifndef MUSIN_BOARDS_DRUM_PIZZA_H
#define MUSIN_BOARDS_DRUM_PIZZA_H

#include <cstdint>
#include <array>
#include <cstddef> // For size_t
#include <optional> // For optional LED return pin
#include "musin/ui/keypad_hc138.h" // Include the keypad driver header
#include "musin/drivers/ws2812.h"       // Include the WS2812 driver header

// No direct SDK includes here, as this file defines the board interface,
// not the microcontroller mapping.
// hardware/pio.h is included by ws2812.h

namespace Musin::Boards {

// --- LED Output Configuration ---
// Moved outside the class so it's available for template instantiation
static constexpr std::uint32_t NUM_LEDS = 37;

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
     * @brief Construct a DrumPizza board interface instance.
     *
     * @param address_pins_gpio Array of 4 GPIO pin numbers corresponding to NamedPin::ADDR_0, ADDR_1, ADDR_2, ADDR_3.
     *                          The first 3 are used by the keypad decoder, all 4 might be used by other components (e.g., analog mux).
     * @param keypad_col_pins_gpio Array of 5 GPIO pin numbers corresponding to NamedPin::RING1 to RING5.
     * @param led_data_pin_gpio GPIO pin number corresponding to NamedPin::LED_DATA.
     * @param led_data_return_pin_gpio Optional GPIO pin number corresponding to NamedPin::LED_DATA_RETURN.
     * @param scan_interval_us Keypad scan interval (microseconds).
     * @param debounce_time_us Keypad debounce time (microseconds).
     * @param hold_time_us Keypad hold time (microseconds).
     */
    DrumPizza(const std::array<std::uint32_t, 4>& address_pins_gpio, // Use std::uint32_t
              const std::array<std::uint32_t, 5>& keypad_col_pins_gpio, // Use std::uint32_t
              std::uint32_t led_data_pin_gpio,                         // Use std::uint32_t
              std::optional<std::uint32_t> led_data_return_pin_gpio = std::nullopt, // Use std::uint32_t
              std::uint32_t scan_interval_us = Musin::UI::Keypad_HC138::DEFAULT_SCAN_INTERVAL_US,
              std::uint32_t debounce_time_us = Musin::UI::Keypad_HC138::DEFAULT_DEBOUNCE_TIME_US,
              std::uint32_t hold_time_us = Musin::UI::Keypad_HC138::DEFAULT_HOLD_TIME_US);

    /**
     * @brief Initialize the hardware components managed by this board class.
     * Must be called after construction, typically once at startup.
     */
    void init();

    /**
     * @brief Get a reference to the keypad driver instance.
     * @return Reference to the Keypad_HC138 object.
     */
    Musin::UI::Keypad_HC138& keypad() { return _keypad; }

    /**
     * @brief Get a const reference to the keypad driver instance.
     * @return Const reference to the Keypad_HC138 object.
     */
    const Musin::UI::Keypad_HC138& keypad() const { return _keypad; }

    /**
     * @brief Get a reference to the WS2812 LED driver instance.
     * @return Reference to the WS2812<Musin::Boards::NUM_LEDS> object.
     */
    Musin::Drivers::WS2812<Musin::Boards::NUM_LEDS>& leds() { return _leds; }

    /**
     * @brief Get a const reference to the WS2812 LED driver instance.
     * @return Const reference to the WS2812<Musin::Boards::NUM_LEDS> object.
     */
    const Musin::Drivers::WS2812<Musin::Boards::NUM_LEDS>& leds() const { return _leds; }


    // --- Named Pin Definitions (Mapped from J1 Connector) ---
    // Represents the logical pins corresponding to the physical J1 connector pins.
    // Ground, Power, and NC pins are included for completeness but might not be
    // directly used in software pin mapping logic.
    enum class NamedPin : std::uint8_t {
        MUX_IO,         // J1 Pin 1
        ADDR_0,         // J1 Pin 5
        ADDR_1,         // J1 Pin 6
        ADDR_2,         // J1 Pin 7
        ADDR_3,         // J1 Pin 10 (Multiplexer Select)
        RING5,          // J1 Pin 11 (Column 5)
        RING4,          // J1 Pin 12 (Column 4)
        RING3,          // J1 Pin 14 (Column 3)
        RING2,          // J1 Pin 15 (Column 2)
        RING1,          // J1 Pin 16 (Column 1)
        LED_DATA_RETURN,// J1 Pin 18
        LED_DATA,       // J1 Pin 19
        _COUNT          // Helper to get the number of enum values if needed
    };

// --- General Configuration ---
// Board revision detection pins (if needed by software) - Uses NamedPin enum
static constexpr NamedPin PIN_BOARD_ADDR0 = NamedPin::ADDR_0; // Example
static constexpr NamedPin PIN_BOARD_ADDR1 = NamedPin::ADDR_1; // Example
static constexpr NamedPin PIN_BOARD_ADDR2 = NamedPin::ADDR_2; // Example

// --- Keypad Configuration ---
static constexpr std::uint8_t KEYPAD_ROWS = 8;
static constexpr std::uint8_t KEYPAD_COLS = 5;

// Note: The static constexpr arrays for NamedPin keypad pins have been removed.
// The actual GPIO pin numbers are now passed to the constructor.

// Symbolic names for columns, matching the physical layout and driver indexing
enum class KeypadColumn : std::uint8_t {
    RING1 = 0, // Drum 1 Sequencer
    RING2 = 1, // Drum 2 Sequencer
    RING3 = 2, // Drum 3 Sequencer
    RING4 = 3, // Drum 4 Sequencer
    RING5 = 4  // Sample Select
};

// --- Analog Input Configuration ---
static constexpr std::uint8_t NUM_ANALOG_INPUTS = 16;

// Analog MUX uses address lines (shared with keypad) and a select line (referenced by NamedPin)
static constexpr NamedPin PIN_MUX_IO       = NamedPin::MUX_IO;     // Shared MUX I/O (J1 Pin 1)
static constexpr NamedPin PIN_MUX_SELECT   = NamedPin::ADDR_3;     // MUX selection (J1 Pin 10)
// MUX Address pins (A0, A1, A2) are shared: keypad_decoder_address_pins

// Enum mapping MUX address (0-15) to control function. This is independent of NamedPin/GPIOs.
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


// LED strip data line (referenced by NamedPin)
static constexpr NamedPin PIN_LED_DATA = NamedPin::LED_DATA; // J1 Pin 19
// static constexpr NamedPin PIN_LED_DATA_RETURN = NamedPin::LED_DATA_RETURN; // Optional (J1 Pin 18)

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

private:
    // Internal buffer for keypad state data
    std::array<Musin::UI::KeyData, KEYPAD_ROWS * KEYPAD_COLS> _key_data_buffer;

    // Keypad driver instance
    Musin::UI::Keypad_HC138 _keypad;

    // LED driver instance (templated with the number of LEDs)
    Musin::Drivers::WS2812<NUM_LEDS> _leds;

    // Pin configuration storage (needed by init or other methods)
    const std::array<std::uint32_t, 4> _address_pins_gpio; // Use std::uint32_t
    const std::uint32_t _led_data_pin_gpio;                // Use std::uint32_t
    const std::optional<std::uint32_t> _led_data_return_pin_gpio; // Use std::uint32_t

    // Add members for other components like Analog Mux driver etc. later

}; // class DrumPizza

} // namespace Musin::Boards

#endif // MUSIN_BOARDS_DRUM_PIZZA_H
