#include "musin/boards/drum_pizza.h"
#include "musin/ui/keypad_hc138.h" // Ensure keypad implementation is available

// Include Pico SDK headers
extern "C" {
#include <stdio.h> // For printf
#include "hardware/gpio.h"
#include "pico/time.h" // For sleep_us
}
#include "musin/ui/ws2812.h"       // Include WS2812 implementation details

namespace Musin::Boards {

enum class ExternalPinState {
    FLOATING,
    PULL_UP,
    PULL_DOWN,
    UNDETERMINED
};

DrumPizza::DrumPizza(const std::array<uint, 4>& address_pins_gpio, // Changed size to 4
                     const std::array<uint, 5>& keypad_col_pins_gpio,
                     uint led_data_pin_gpio,
                     std::optional<uint> led_data_return_pin_gpio,
                     std::uint32_t scan_interval_us,
                     std::uint32_t debounce_time_us,
                     std::uint32_t hold_time_us)
    : // Initialize _key_data_buffer implicitly (default constructor for array elements)
      _keypad(KEYPAD_ROWS, // Member initialization order matters
              KEYPAD_COLS,
              {address_pins_gpio[0], address_pins_gpio[1], address_pins_gpio[2]}, // Pass only first 3 pins to keypad
              keypad_col_pins_gpio.data(), // Pass pointer to the underlying array data
              _key_data_buffer.data(),     // Pass pointer to the internal buffer
              scan_interval_us,
              debounce_time_us,
              hold_time_us),
      // Initialize _leds before _keypad_addr_pins_gpio etc. if declared earlier in header
      _leds(led_data_pin_gpio,                    // LED data pin GPIO number (PIO/SM determined in _leds.init())
            led_data_return_pin_gpio.has_value() ? (NUM_LEDS + 1) : NUM_LEDS, // Number of LEDs
            Musin::UI::RGBOrder::GRB,             // Color order
            255,                                  // Initial brightness (will be corrected in init)
            std::nullopt),                        // No color correction for now
      _address_pins_gpio(address_pins_gpio), // Store all 4 address pins
      _led_data_pin_gpio(led_data_pin_gpio),         // Store LED data pin
      _led_data_return_pin_gpio(led_data_return_pin_gpio) // Store optional return pin
{
    // Constructor body:
    // - _keypad is initialized above.
    // - _leds is initialized above with default brightness.
    // - Pin numbers are stored.
    // Actual hardware init and brightness setting happens in init().
}


// Helper function to check external pin state
// Returns the detected state.
static ExternalPinState check_external_pin_state(uint gpio, const char* name) {
    // 1. Initialize pin as input
    // Note: gpio_init is safe to call multiple times
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
    ExternalPinState determined_state;
    const char* state_str; // For printing

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
        // e.g., initial=0, pullup=1, pulldown=1 (weak pull-down?)
        // e.g., initial=1, pullup=0, pulldown=0 (weak pull-up?)
        determined_state = ExternalPinState::UNDETERMINED;
        state_str = "Undetermined / Inconsistent Reads";
    }

    printf("DrumPizza Init: Pin %u (%s) external state check result: %s\n", gpio, name, state_str);

    // 9. Disable pulls again before returning/proceeding
    gpio_disable_pulls(gpio);
    sleep_us(10); // Allow time for pulls to disable if needed

    return determined_state;
}


void DrumPizza::init() {
    printf("DrumPizza Initializing...\n");

    // --- Check initial external pin states before configuring components ---
    printf("DrumPizza: Checking external pin states...\n");
    // Address Pins (ADDR_0, ADDR_1, ADDR_2, ADDR_3) - Check state for info
    check_external_pin_state(_address_pins_gpio[0], "ADDR_0");
    check_external_pin_state(_address_pins_gpio[1], "ADDR_1");
    check_external_pin_state(_address_pins_gpio[2], "ADDR_2");
    check_external_pin_state(_address_pins_gpio[3], "ADDR_3"); // Check the 4th address pin

    // LED Data Pin - Check state to determine brightness
    ExternalPinState led_pin_state = check_external_pin_state(_led_data_pin_gpio, "LED_DATA");

    // --- Initialize Components ---

    // Keypad
    printf("DrumPizza: Initializing Keypad...\n");
    _keypad.init(); // Configures keypad pins (addr=out, col=in+pullup)

    // LEDs
    printf("DrumPizza: Initializing LEDs...\n");
    // Set brightness based on pin state check
    // If the pin is pulled up, assume SK6812, so 12mA per channel. Set brighness to 100
    // If it is pulled down, assume SK6805, so 5mA per channel. Set brightness to full
    uint8_t initial_brightness = (led_pin_state == ExternalPinState::PULL_UP) ? 100 : 255;
    printf("DrumPizza: Setting initial LED brightness to %u (based on pin state: %d)\n",
           initial_brightness, static_cast<int>(led_pin_state));
    _leds.set_brightness(initial_brightness);

    // Initialize the WS2812 driver (configures PIO and LED pin)
    if (!_leds.init()) {
        // Handle LED initialization failure (e.g., print error, panic)
        printf("Error: Failed to initialize WS2812 LED driver!\n");
        // Depending on requirements, might panic here: panic("WS2812 Init Failed");
    } else {
        // Optional: Clear LEDs on startup
        _leds.clear();
        _leds.show();
    }

    // Initialize other components here (e.g., Analog Mux) when added

    printf("DrumPizza Initialization Complete.\n");
}

// The keypad() getter is defined inline in the header.

} // namespace Musin::Boards
