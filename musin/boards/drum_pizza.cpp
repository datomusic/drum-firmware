#include "musin/boards/drum_pizza.h"
#include "musin/ui/keypad_hc138.h" // Ensure keypad implementation is available

// Include Pico SDK headers
extern "C" {
#include <stdio.h> // For printf
#include "hardware/gpio.h"
#include "pico/time.h" // For sleep_us
}

namespace Musin::Boards {

DrumPizza::DrumPizza(const std::array<uint, 3>& keypad_addr_pins_gpio,
                     const std::array<uint, 5>& keypad_col_pins_gpio,
                     uint led_data_pin_gpio,
                     std::optional<uint> led_data_return_pin_gpio,
                     std::uint32_t scan_interval_us,
                     std::uint32_t debounce_time_us,
                     std::uint32_t hold_time_us)
    : // Initialize _key_data_buffer implicitly (default constructor for array elements)
      _keypad(KEYPAD_ROWS, // Member initialization order matters
              KEYPAD_COLS,
              keypad_addr_pins_gpio,
              keypad_col_pins_gpio.data(), // Pass pointer to the underlying array data
              _key_data_buffer.data(),     // Pass pointer to the internal buffer
              scan_interval_us,
              debounce_time_us,
              hold_time_us),
      _led_data_pin_gpio(led_data_pin_gpio),
      _led_data_return_pin_gpio(led_data_return_pin_gpio)
{
    // Constructor body (if needed for other initializations)
}

// Helper function to check and print external pin state
static void print_external_pin_state(uint gpio, const char* name) {
    // 1. Initialize pin as input
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);

    // 2. Disable internal pulls
    gpio_disable_pulls(gpio);
    sleep_us(10); // Small delay to allow pin state to settle

    // 3. Read initial state (no internal pulls)
    bool initial_read = gpio_get(gpio);
    // printf("DEBUG: Pin %u (%s) - Initial read (no pulls): %d\n", gpio, name, initial_read);

    // 4. Enable internal pull-up
    gpio_pull_up(gpio);
    sleep_us(10); // Delay

    // 5. Read with pull-up
    bool pullup_read = gpio_get(gpio);
    // printf("DEBUG: Pin %u (%s) - Read with pull-up: %d\n", gpio, name, pullup_read);

    // 6. Enable internal pull-down
    gpio_pull_down(gpio); // Disables pull-up automatically
    sleep_us(10); // Delay

    // 7. Read with pull-down
    bool pulldown_read = gpio_get(gpio);
    // printf("DEBUG: Pin %u (%s) - Read with pull-down: %d\n", gpio, name, pulldown_read);

    // 8. Determine external state based on readings
    const char* external_state;
    if (!initial_read && pullup_read) {
        // Initially low, pulled high by internal pull-up.
        // If it's also pulled low by internal pull-down, it's floating.
        if (!pulldown_read) {
             external_state = "Floating";
        } else {
             // Stays high with pull-down? Unexpected, maybe very weak pull-down fighting something?
             external_state = "Floating (or Weak Pull-down)"; // Best guess
        }
    } else if (!initial_read && !pullup_read) {
        // Initially low, stays low even with internal pull-up -> Strong External Pull-down
        external_state = "External Pull-down";
    } else if (initial_read && !pulldown_read) {
        // Initially high, pulled low by internal pull-down.
        // If it also reads high with internal pull-up, it's floating.
         if (pullup_read) {
             external_state = "Floating";
         } else {
             // Stays low with pull-up? Unexpected, maybe very weak pull-up fighting something?
             external_state = "Floating (or Weak Pull-up)"; // Best guess
         }
    } else if (initial_read && pulldown_read) {
        // Initially high, stays high even with internal pull-down -> Strong External Pull-up
        external_state = "External Pull-up";
    } else {
        // Covers cases like: initial=0, pullup=0, pulldown=1 or initial=1, pullup=0, pulldown=1
        external_state = "Undetermined / Inconsistent Reads";
    }

    printf("DrumPizza Init: Pin %u (%s) external state: %s\n", gpio, name, external_state);

    // 9. Disable pulls again before returning/proceeding
    gpio_disable_pulls(gpio);
    sleep_us(10);
}


void DrumPizza::init() {
    printf("DrumPizza Initializing...\n");

    // --- Check initial external pin states before configuring them ---
    printf("DrumPizza: Checking external pin states...\n");
    // Keypad Address Pins (ADDR_0, ADDR_1, ADDR_2)
    // Note: We need access to the keypad address pins here. They should be stored as members.
    // Assuming _keypad_addr_pins_gpio is a member std::array<uint, 3>
    print_external_pin_state(_keypad_addr_pins_gpio[0], "ADDR_0");
    print_external_pin_state(_keypad_addr_pins_gpio[1], "ADDR_1");
    print_external_pin_state(_keypad_addr_pins_gpio[2], "ADDR_2");

    // LED Data Pin
    print_external_pin_state(_led_data_pin_gpio, "LED_DATA");

    // --- Initialize Components ---
    printf("DrumPizza: Initializing Keypad...\n");
    // Initialize the keypad GPIOs (this will set address pins to output)
    _keypad.init();

    printf("DrumPizza: Initializing LEDs...\n");
    // TODO: Initialize LED driver here using _led_data_pin_gpio
    // For now, just ensure the pin is initialized if not done elsewhere
    gpio_init(_led_data_pin_gpio);
    // LED driver will set direction later (likely output)

    // Initialize other components here (e.g., Analog Mux) when added

    printf("DrumPizza Initialization Complete.\n");
}

// The keypad() getter is defined inline in the header.

} // namespace Musin::Boards
