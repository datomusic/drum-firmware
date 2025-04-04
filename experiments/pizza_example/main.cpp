#include <stdio.h>
#include <array>
#include <optional>
#include "pico/stdlib.h"
#include "musin/boards/drum_pizza.h"
#include "musin/ui/keypad_hc138.h" // For KeyState definition if needed
#include "musin/drivers/ws2812.h"       // For WS2812 definition if needed

// --- Pin Mapping (Example for Raspberry Pi Pico) ---
// Adjust these based on your actual wiring between Pico and Drum Pizza J1
constexpr unsigned int PIN_ADDR_0 = 29;
constexpr unsigned int PIN_ADDR_1 = 6;
constexpr unsigned int PIN_ADDR_2 = 7;
constexpr unsigned int PIN_ADDR_3 = 9;

constexpr unsigned int PIN_RING_1 = 15;
constexpr unsigned int PIN_RING_2 = 14;
constexpr unsigned int PIN_RING_3 = 13;
constexpr unsigned int PIN_RING_4 = 11;
constexpr unsigned int PIN_RING_5 = 10;

constexpr unsigned int PIN_LED_DATA_OUT = 16; // J1 Pin 19
constexpr unsigned int PIN_LED_ENABLE = 20;   // GPIO for enabling LED power

const std::optional<unsigned int> led_return_pin = std::nullopt; // Use std::nullopt if not connected
// const std::optional<unsigned int> led_return_pin = PIN_LED_DATA_RET; // Use if connected

// --- Main Application ---
int main() {
    // Initialize standard libraries
    stdio_init_all();
    printf("Starting Drum Pizza Example...\n");

    // --- Configure Board Pins ---
    // Address pins (4 needed for analog mux, first 3 used by keypad)
    const std::array<unsigned int, 4> address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2, PIN_ADDR_3};
    const std::array<unsigned int, 5> keypad_col_pins = {PIN_RING_1, PIN_RING_2, PIN_RING_3, PIN_RING_4, PIN_RING_5};

    // --- Create Board Instance ---
    // This constructor initializes the keypad and LED driver members internally
    Musin::Boards::DrumPizza board(
        address_pins, // Pass the 4 address pins
        keypad_col_pins,
        PIN_LED_DATA_OUT,
        led_return_pin
        // Using default timings for keypad scan/debounce/hold
    );

    // --- Initialize Board Hardware ---
    // This calls init() on the keypad and LED drivers, configuring GPIOs and PIO
    printf("Initializing board hardware...\n");
    board.init();
    printf("Board initialization complete.\n");

    // --- Enable LED Power ---
    printf("Enabling LED power (GPIO %u)...\n", PIN_LED_ENABLE);
    gpio_init(PIN_LED_ENABLE);
    gpio_set_dir(PIN_LED_ENABLE, GPIO_OUT);
    gpio_put(PIN_LED_ENABLE, 1); // Set high to enable

    // --- LED Chaser State ---
    unsigned int current_led_index = 0;
    unsigned int num_board_leds = board.leds().get_num_leds(); // Get actual number managed by driver
    uint32_t loop_delay_ms = 50; // Delay between chaser steps

    // --- Main Loop ---
    while (true) {
        // --- Scan Keypad ---
        if (board.keypad().scan()) {
            // Scan was performed, check for presses
            for (uint8_t r = 0; r < board.keypad().get_num_rows(); ++r) {
                for (uint8_t c = 0; c < board.keypad().get_num_cols(); ++c) {
                    if (board.keypad().was_pressed(r, c)) {
                        printf("Key Pressed: Row %u, Col %u\n", r, c);
                    }
                    // Optional: Check for release or hold events
                    // if (board.keypad().was_released(r, c)) {
                    //     printf("Key Released: Row %u, Col %u\n", r, c);
                    // }
                    // if (board.keypad().is_held(r, c)) {
                    //     // Note: is_held is continuous, might print repeatedly
                    // }
                }
            }
        }

        // --- Update LED Chaser ---
        // Turn off the previous LED
        unsigned int prev_led_index = (current_led_index == 0) ? (num_board_leds - 1) : (current_led_index - 1);
        board.leds().set_pixel(prev_led_index, 0, 0, 0); // Black

        // Turn on the current LED (white)
        board.leds().set_pixel(current_led_index, 255, 255, 255); // White

        // Send data to the LED strip
        board.leds().show();

        // Move to the next LED
        current_led_index = (current_led_index + 1) % num_board_leds;

        // --- Loop Delay ---
        sleep_ms(loop_delay_ms);
    }

    return 0; // Should not be reached
}
