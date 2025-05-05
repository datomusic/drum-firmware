#include "musin/boards/drum_pizza.h"
#include "musin/ui/keypad_hc138.h" // Ensure keypad implementation is available

// Include Pico SDK headers
extern "C" {
#include <stdio.h> // For printf
#include "hardware/gpio.h"
#include "pico/time.h" // For sleep_us
}
#include "musin/drivers/ws2812.h"   // Updated include path for WS2812

namespace musin::Boards {

enum class ExternalPinState {
    FLOATING,
    PULL_UP,
    PULL_DOWN,
    UNDETERMINED
};

DrumPizza::DrumPizza(const std::array<std::uint32_t, 4>& address_pins_gpio, // Use std::uint32_t
                     const std::array<std::uint32_t, 5>& keypad_col_pins_gpio, // Use std::uint32_t
                     std::uint32_t led_data_pin_gpio,                         // Use std::uint32_t
                     std::optional<std::uint32_t> led_data_return_pin_gpio, // Use std::uint32_t
                     std::uint32_t scan_interval_us,
                     std::uint32_t debounce_time_us,
                     std::uint32_t hold_time_us)
    : // Initialize _key_data_buffer implicitly (default constructor for array elements)
      _keypad(KEYPAD_ROWS, // Member initialization order matters
              KEYPAD_COLS,
              // Keypad_HC138 constructor expects std::array<uint, 3> and const uint*
              // We need to ensure the types match or provide a conversion/cast if safe.
              // Assuming Keypad_HC138 constructor still expects `unsigned int` based on its header.
              // Let's create temporary arrays with the correct type for the keypad constructor.
              // This is slightly less efficient but ensures type safety without changing Keypad_HC138 yet.
              {static_cast<unsigned int>(address_pins_gpio[0]),
               static_cast<unsigned int>(address_pins_gpio[1]),
               static_cast<unsigned int>(address_pins_gpio[2])},
              // Need to convert keypad_col_pins_gpio to const uint*
              // Create a temporary array of uint for this purpose.
              [keypad_col_pins_gpio]() -> std::array<unsigned int, 5> {
                  std::array<unsigned int, 5> temp_cols;
                  for(size_t i=0; i<5; ++i) temp_cols[i] = static_cast<unsigned int>(keypad_col_pins_gpio[i]);
                  return temp_cols;
              }().data(), // Pass pointer to the temporary array's data
              _key_data_buffer.data(),     // Pass pointer to the internal buffer
              scan_interval_us,
              debounce_time_us,
              hold_time_us),
      _leds(led_data_pin_gpio, // Argument 1: data pin (now std::uint32_t)
            musin::drivers::RGBOrder::GRB, // Argument 2: order
            255, // Argument 3: initial brightness
            std::nullopt), // Argument 4: color correction
      _address_pins_gpio(address_pins_gpio), // Store the std::uint32_t array
      _led_data_pin_gpio(led_data_pin_gpio), // Store the std::uint32_t pin
      _led_data_return_pin_gpio(led_data_return_pin_gpio) // Store the optional std::uint32_t
{
}


static ExternalPinState check_external_pin_state(std::uint32_t gpio, const char* name) { // Use std::uint32_t
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

    printf("DrumPizza Init: Pin %lu (%s) external state check result: %s\n", gpio, name, state_str);

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

} // namespace musin::Boards
