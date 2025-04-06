#include <stdio.h>
#include <array>
#include <vector> // Include vector for potential future use or if other headers need it indirectly
#include <optional> // For std::optional

// Pico SDK Headers
extern "C" {
#include "pico/stdlib.h"
#include "pico/time.h"
// hardware/adc.h and hardware/gpio.h are included by analog_in.cpp
// hardware/pio.h is included by ws2812.h/cpp
}

// Musin Headers
#include "musin/boards/drum_pizza.h"
#include "musin/hal/analog_in.h" // Includes AnalogIn and AnalogInMux
#include "musin/ui/drumpad.h"    // Includes Drumpad template

#include <cstdint> // Include for std::uint32_t

// --- Pin Mapping (Adjust to your hardware wiring) ---
// Define GPIO pins connected to the DrumPizza J1 connector based on a common setup
// Use std::uint32_t for pin types to match constructor expectations
constexpr std::uint32_t PIN_MUX_IO = 26; // ADC0

constexpr std::uint32_t PIN_ADDR_0 = 10; // Mux S0 / Keypad A0
constexpr std::uint32_t PIN_ADDR_1 = 11; // Mux S1 / Keypad A1
constexpr std::uint32_t PIN_ADDR_2 = 12; // Mux S2 / Keypad A2
constexpr std::uint32_t PIN_ADDR_3 = 13; // Mux S3 / Mux Select

constexpr std::uint32_t PIN_RING_1 = 20; // Keypad Col 0 (Ring 1)
constexpr std::uint32_t PIN_RING_2 = 19; // Keypad Col 1 (Ring 2)
constexpr std::uint32_t PIN_RING_3 = 18; // Keypad Col 2 (Ring 3)
constexpr std::uint32_t PIN_RING_4 = 17; // Keypad Col 3 (Ring 4)
constexpr std::uint32_t PIN_RING_5 = 16; // Keypad Col 4 (Ring 5)

constexpr std::uint32_t PIN_LED_DATA = 21;
// constexpr uint PIN_LED_DATA_RETURN = 22; // Optional return pin

// --- Main Application ---
int main() {
    // Initialize standard libraries
    stdio_init_all();
    // Add a small delay to allow serial connection to establish after flashing
    sleep_ms(2000);
    printf("==============================\n");
    printf(" Starting Drumpad Test Example\n");
    printf("==============================\n");

    // --- Configure Board Pins ---
    // Use all 4 address pins for DrumPizza constructor, even if keypad only uses 3
    // Use std::uint32_t for the array element type
    const std::array<std::uint32_t, 4> address_pins_gpio = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2, PIN_ADDR_3};
    const std::array<std::uint32_t, 5> keypad_col_pins_gpio = {PIN_RING_1, PIN_RING_2, PIN_RING_3, PIN_RING_4, PIN_RING_5};

    // --- Instantiate Board ---
    // DrumPizza handles keypad and LED setup via its init()
    printf("Instantiating DrumPizza board...\n");
    // Pass the std::uint32_t arrays to the DrumPizza constructor
    Musin::Boards::DrumPizza board(
        address_pins_gpio,
        keypad_col_pins_gpio,
        PIN_LED_DATA
        // Not using LED return pin in this example: std::nullopt
    );

    // --- Instantiate Analog Readers for Drumpads ---
    // Drumpads use the 16-channel mux configuration (AnalogInMux<4>)
    // All readers share the same ADC pin and address pins, but differ by channel address.
    printf("Instantiating AnalogInMux16 readers...\n");
    // We pass the *same* address pin array to all readers.
    Musin::HAL::AnalogInMux16 reader_drum1(
        PIN_MUX_IO, address_pins_gpio, static_cast<uint8_t>(Musin::Boards::DrumPizza::AnalogInput::DRUM1)
    );
    Musin::HAL::AnalogInMux16 reader_drum2(
        PIN_MUX_IO, address_pins_gpio, static_cast<uint8_t>(Musin::Boards::DrumPizza::AnalogInput::DRUM2)
    );
    Musin::HAL::AnalogInMux16 reader_drum3(
        PIN_MUX_IO, address_pins_gpio, static_cast<uint8_t>(Musin::Boards::DrumPizza::AnalogInput::DRUM3)
    );
    Musin::HAL::AnalogInMux16 reader_drum4(
        PIN_MUX_IO, address_pins_gpio, static_cast<uint8_t>(Musin::Boards::DrumPizza::AnalogInput::DRUM4)
    );

    // --- Instantiate Drumpad Drivers ---
    // Pass the corresponding analog reader instance by reference.
    // The Drumpad template needs the specific reader type.
    printf("Instantiating Drumpad drivers...\n");
    Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> pad1(reader_drum1);
    Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> pad2(reader_drum2);
    Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> pad3(reader_drum3);
    Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> pad4(reader_drum4);

    // Store pads in an array for easier iteration
    std::array<Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>*, 4> drumpads = {&pad1, &pad2, &pad3, &pad4};
    // Store readers in an array for easier initialization
    std::array<Musin::HAL::AnalogInMux16*, 4> analog_readers = {&reader_drum1, &reader_drum2, &reader_drum3, &reader_drum4};


    // --- Initialize Hardware ---
    printf("Initializing DrumPizza board (Keypad, LEDs)...\n");
    board.init(); // Initializes Keypad pins and LED PIO

    printf("Initializing Analog Readers (ADC, Mux Pins)...\n");
    for (auto reader : analog_readers) {
        reader->init(); // Initializes ADC pin and address pins for each reader instance
    }
    // Drumpad instances don't have their own init() - they rely on the reader being initialized.

    printf("Initialization complete. Entering main loop.\n");
    printf("Hit the pads!\n");

    // --- Main Loop ---
    while (true) {
        // Update all drumpads
        for (size_t i = 0; i < drumpads.size(); ++i) {
            // update() returns true if a scan was performed in this call
            if (drumpads[i]->update()) {
                // Check for press event *after* update
                if (drumpads[i]->was_pressed()) {
                    std::optional<uint8_t> velocity = drumpads[i]->get_velocity();
                    if (velocity) {
                        printf("Pad %zu Pressed! Velocity: %3d (Raw Peak Est: %4d)\n",
                               i + 1, // Pad number 1-4
                               *velocity,
                               drumpads[i]->get_raw_adc_value()); // Note: raw value is last read, not necessarily peak
                    } else {
                        // This case might occur if the signal drops very quickly
                        // between velocity low and high thresholds crossing checks.
                        printf("Pad %zu Pressed! (Velocity calculation failed? Raw: %d)\n",
                               i + 1,
                               drumpads[i]->get_raw_adc_value());
                    }
                }
                // Optional: Check for release or hold events
                // if (drumpads[i]->was_released()) {
                //     printf("Pad %zu Released (Raw: %d)\n", i + 1, drumpads[i]->get_raw_adc_value());
                // }
            }
        }

        // Let other tasks run (like USB background tasks if stdio_usb is used)
        // Or just prevent the loop from consuming 100% CPU if scans are fast.
        // A small sleep is often sufficient if not using an RTOS.
        // tight_loop_contents(); // Use this if you need extremely tight timing
        sleep_us(100); // Sleep for 100 microseconds
    }

    return 0; // Should never reach here
}
