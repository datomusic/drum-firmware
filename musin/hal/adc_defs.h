#ifndef MUSIN_HAL_ADC_DEFS_H
#define MUSIN_HAL_ADC_DEFS_H

#include "etl/array.h"
#include <cstdint>
#include "pico/assert.h"

namespace musin::hal {

// --- Public Constants ---
static constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;
static constexpr uint16_t ADC_MAX_VALUE = 4095;

/**
 * @brief Converts a GPIO pin number to its corresponding ADC channel number at compile time.
 * @param pin GPIO pin number (must be 26-29).
 * @return ADC channel number (0-3).
 */
constexpr std::uint32_t pin_to_adc_channel(std::uint32_t pin) {
  // The valid input range is 26-29. This is enforced by the hardware design.
  return pin - 26;
}

/**
 * @brief Sets the state of multiple GPIO pins based on a binary value.
 * Used to control multiplexer address lines.
 * @tparam Container Type of the container holding address pins (e.g., etl::array).
 * @param address_pins Container of GPIO pin numbers (index 0 = LSB).
 * @param address_value The address value to set.
 */
template <typename Container>
void set_mux_address(const Container &address_pins, uint8_t address_value);

// Explicit template instantiation declarations
extern template void set_mux_address<etl::array<std::uint32_t, 3>>(const etl::array<std::uint32_t, 3> &, uint8_t);
extern template void set_mux_address<etl::array<std::uint32_t, 4>>(const etl::array<std::uint32_t, 4> &, uint8_t);


} // namespace musin::hal

#endif // MUSIN_HAL_ADC_DEFS_H