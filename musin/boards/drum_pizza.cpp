#include "musin/boards/drum_pizza.h"
#include "musin/ui/keypad_hc138.h" // Ensure keypad implementation is available

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


void DrumPizza::init() {
    // Initialize the keypad GPIOs
    _keypad.init();

    // Initialize other components here (e.g., Analog Mux, LEDs) when added
}

// The keypad() getter is defined inline in the header.

} // namespace Musin::Boards
