#ifndef MUSIN_HAL_ANALOG_IN_H
#define MUSIN_HAL_ANALOG_IN_H

#include <cstdint>

// Wrap C SDK headers
extern "C" {
#include "hardware/adc.h"
#include "pico/assert.h" // For assertions
}

namespace Musin::HAL {

/**
 * @brief Provides a simple interface for reading a single analog input pin.
 *
 * This class handles the initialization and reading of a specific ADC channel
 * associated with a GPIO pin.
 */
class AnalogIn {
public:
  /**
   * @brief Construct an AnalogIn instance for a specific GPIO pin.
   *
   * @param pin The GPIO pin number (must be ADC capable: 26, 27, 28, or 29 for temp sensor).
   * @param enable_temp_sensor If true and pin is 29, enables the temperature sensor.
   *                           Defaults to false. Ignored for pins other than 29.
   */
  explicit AnalogIn(uint pin, bool enable_temp_sensor = false);

  // Prevent copying and assignment
  AnalogIn(const AnalogIn&) = delete;
  AnalogIn& operator=(const AnalogIn&) = delete;

  /**
   * @brief Initialize the ADC hardware and the specific GPIO pin for ADC input.
   * Must be called once before reading.
   */
  void init();

  /**
   * @brief Read the analog value from the configured pin.
   *
   * Selects the appropriate ADC channel, performs a conversion, and returns the result.
   * The RP2040 ADC is 12-bit, so this value is scaled to fit a 16-bit unsigned integer
   * by left-shifting by 4 bits (`result << 4`). This provides a consistent 16-bit range
   * while preserving the relative resolution.
   *
   * @return The 16-bit representation of the analog reading (0-65520 in steps of 16).
   *         Returns 0 if the class has not been initialized.
   */
  std::uint16_t read() const;

  /**
   * @brief Read the raw 12-bit ADC value.
   *
   * @return The raw 12-bit ADC reading (0-4095).
   *         Returns 0 if the class has not been initialized.
   */
  std::uint16_t read_raw() const;

private:
  const uint _pin;
  const uint _adc_channel;
  const bool _enable_temp_sensor;
  bool _initialized = false;

  /**
   * @brief Converts a GPIO pin number to its corresponding ADC channel number.
   * @param pin GPIO pin number (26-29).
   * @return ADC channel number (0-4), or asserts if the pin is invalid.
   */
  static uint pin_to_adc_channel(uint pin);
};

} // namespace Musin::HAL

#endif // MUSIN_HAL_ANALOG_IN_H
