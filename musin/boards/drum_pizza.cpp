#include "musin/boards/drum_pizza.h"
#include "musin/ui/keypad_hc138.h" // Ensure keypad implementation is available

// Include Pico SDK headers
extern "C" {
#include <stdio.h> // For printf
#include "hardware/gpio.h"
#include "pico/time.h" // For sleep_us
}
#include "musin/hal/ws2812.h"       // Include WS2812 implementation details

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
      _leds(led_data_pin_gpio,
            led_data_return_pin_gpio.has_value() ? (NUM_LEDS + 1) : NUM_LEDS,
            Musin::HAL::RGBOrder::GRB,
            255, // Initial brightness (will be corrected in init)
            std::nullopt),
      _address_pins_gpio(address_pins_gpio),
      _led_data_pin_gpio(led_data_pin_gpio),
      _led_data_return_pin_gpio(led_data_return_pin_gpio)
{
}


static ExternalPinState check_external_pin_state(uint gpio, const char* name) {
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

    printf("DrumPizza Init: Pin %u (%s) external state check result: %s\n", gpio, name, state_str);

    gpio_disable_pulls(gpio);
    sleep_us(10);

    return determined_state;
}


void DrumPizza::init() {
    printf("DrumPizza Initializing...\n");

    printf("DrumPizza: Checking external pin states...\n");
    check_external_pin_state(_address_pins_gpio[0], "ADDR_0");
    check_external_pin_state(_address_pins_gpio[1], "ADDR_1");
    check_external_pin_state(_address_pins_gpio[2], "ADDR_2");
    check_external_pin_state(_address_pins_gpio[3], "ADDR_3");

    ExternalPinState led_pin_state = check_external_pin_state(_led_data_pin_gpio, "LED_DATA");

    printf("DrumPizza: Initializing Keypad...\n");
    _keypad.init();

    printf("DrumPizza: Initializing LEDs...\n");
    // Set brightness based on pin state check
    // If the pin is pulled up, assume SK6812, so 12mA per channel. Set brighness to 100
    // If it is pulled down, assume SK6805, so 5mA per channel. Set brightness to full
    uint8_t initial_brightness = (led_pin_state == ExternalPinState::PULL_UP) ? 100 : 255;
    printf("DrumPizza: Setting initial LED brightness to %u (based on pin state: %d)\n",
           initial_brightness, static_cast<int>(led_pin_state));
    _leds.set_brightness(initial_brightness);

    if (!_leds.init()) {
        printf("Error: Failed to initialize WS2812 LED driver!\n");
        // Depending on requirements, might panic here: panic("WS2812 Init Failed");
    } else {
        _leds.clear();
        _leds.show();
    }

    printf("DrumPizza Initialization Complete.\n");
}

} // namespace Musin::Boards
