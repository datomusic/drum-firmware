#include "musin/hal/adc_defs.h"

extern "C" {
#include "hardware/gpio.h"
#include "pico/assert.h"
}

namespace musin::hal {

template <typename Container>
void set_mux_address(const Container &address_pins, uint8_t address_value) {
  for (size_t i = 0; i < address_pins.size(); ++i) {
    if (i >= sizeof(address_value) * 8)
      break;
    gpio_put(address_pins[i], (address_value >> i) & 1);
  }
}

// Explicit template instantiation definitions
template void set_mux_address<etl::array<std::uint32_t, 3>>(const etl::array<std::uint32_t, 3> &, uint8_t);
template void set_mux_address<etl::array<std::uint32_t, 4>>(const etl::array<std::uint32_t, 4> &, uint8_t);

} // namespace musin::hal