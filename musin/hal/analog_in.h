#ifndef MUSIN_HAL_ANALOG_IN_H
#define MUSIN_HAL_ANALOG_IN_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace musin::hal {

/**
 * @brief Provides a simple interface for reading a single analog input pin.
 *
 * This class handles the initialization and reading of a specific ADC channel
 * associated with a GPIO pin.
 */
class AnalogIn {
public:
  // --- Public Constants ---
  // ADC reference voltage (typically 3.3V for RP2040)
  static constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;
  // Maximum raw ADC value (12-bit)
  static constexpr float ADC_MAX_VALUE = 4095.0f;

  /**
   * @brief Construct an AnalogIn instance for a specific GPIO pin.
   *
   * @param pin The GPIO pin number (must be ADC capable: 26, 27, 28, or 29 for temp sensor).
   * @param enable_temp_sensor If true and pin is 29, enables the temperature sensor.
   *                           Defaults to false. Ignored for pins other than 29.
   */
  explicit AnalogIn(std::uint32_t pin, bool enable_temp_sensor = false);

  AnalogIn(const AnalogIn &) = delete;
  AnalogIn &operator=(const AnalogIn &) = delete;

  /**
   * @brief Initialize the ADC hardware and the specific GPIO pin for ADC input.
   * Must be called once before reading.
   */
  void init();

  /**
   * @brief Read the normalized analog value from the configured pin.
   *
   * Selects the appropriate ADC channel, performs a conversion, reads the raw 12-bit value,
   * and returns it normalized to a float between 0.0f and 1.0f.
   *
   * @return The normalized analog reading as a float (0.0f to 1.0f).
   *         Returns 0.0f if the class has not been initialized.
   */
  float read() const;

  /**
   * @brief Read the raw 12-bit ADC value.
   *
   * @return The raw 12-bit ADC reading (0-4095).
   *         Returns 0 if the class has not been initialized.
   */
  std::uint16_t read_raw() const;

  /**
   * @brief Read the analog value and convert it to voltage.
   *
   * Assumes the standard ADC reference voltage (typically 3.3V).
   *
   * @return The calculated voltage as a float.
   *         Returns 0.0f if the class has not been initialized.
   */
  float read_voltage() const;

private:
  const std::uint32_t _pin;
  const std::uint32_t _adc_channel;
  const bool _enable_temp_sensor;
  bool _initialized = false;
};

/**
 * @brief Converts a GPIO pin number to its corresponding ADC channel number (RP2040 specific).
 * @param pin GPIO pin number (26-29).
 * @return ADC channel number (0-4), or asserts if the pin is invalid.
 */
std::uint32_t pin_to_adc_channel(std::uint32_t pin);

/**
 * @brief Sets the state of multiple GPIO pins based on a binary value.
 * Used to control multiplexer address lines.
 * @tparam Container Type of the container holding address pins (e.g., std::array).
 * @param address_pins Container of GPIO pin numbers (index 0 = LSB).
 * @param address_value The address value to set.
 */
template <typename Container>
void set_mux_address(const Container &address_pins, uint8_t address_value);

/**
 * @brief Generic interface for reading an analog input pin via a multiplexer.
 * @tparam NumAddressPins The number of address lines required by the multiplexer (e.g., 3 for
 * 8-channel, 4 for 16-channel).
 */
template <size_t NumAddressPins> class AnalogInMux {
public:
  static constexpr uint8_t MAX_CHANNELS = (1 << NumAddressPins);
  static_assert(NumAddressPins > 0 && NumAddressPins <= 4,
                "AnalogInMux supports 1 to 4 address pins (2 to 16 channels)");

  /**
   * @brief Construct an AnalogInMux instance.
   *
   * @param adc_pin The GPIO pin connected to the ADC input (must be ADC capable: 26, 27, 28).
   * @param address_pins Array containing the GPIO pin numbers for the mux address lines (index
   * 0=LSB/S0, ..., NumAddressPins-1=MSB).
   * @param channel_address The specific channel (0 to MAX_CHANNELS - 1) on the multiplexer for this
   * input.
   * @param address_settle_time_us Microseconds to wait after setting address pins before reading
   * ADC.
   */
  AnalogInMux(std::uint32_t adc_pin, const std::array<std::uint32_t, NumAddressPins> &address_pins,
              uint8_t channel_address, std::uint32_t address_settle_time_us = 5);

  // Prevent copying and assignment
  AnalogInMux(const AnalogInMux &) = delete;
  AnalogInMux &operator=(const AnalogInMux &) = delete;

  /**
   * @brief Initialize the ADC, ADC GPIO pin, and address GPIO pins.
   * Must be called once before reading.
   */
  void init();

  /**
   * @brief Read the normalized analog value from the configured mux channel.
   * Sets the mux address, waits briefly, reads the raw 12-bit ADC value,
   * and returns it normalized to a float between 0.0f and 1.0f.
   * @return The normalized analog reading as a float (0.0f to 1.0f). Returns 0.0f if not
   * initialized.
   */
  float read() const;

  /**
   * @brief Read the raw 12-bit analog value from the configured mux channel.
   * Sets the mux address, waits briefly, then reads the ADC.
   * @return The raw 12-bit ADC reading (0-4095). Returns 0 if not initialized.
   */
  std::uint16_t read_raw() const;

  /**
   * @brief Read the analog value and convert it to voltage.
   * Sets the mux address, waits briefly, then reads the ADC.
   * @return The calculated voltage as a float. Returns 0.0f if not initialized.
   */
  float read_voltage() const;

private:
  const std::uint32_t _adc_pin;
  const std::uint32_t _adc_channel;
  const std::array<std::uint32_t, NumAddressPins> _address_pins;
  const uint8_t _channel_address;
  const std::uint32_t _address_settle_time_us;
  bool _initialized = false;

  static constexpr float ADC_REFERENCE_VOLTAGE = AnalogIn::ADC_REFERENCE_VOLTAGE;
  static constexpr float ADC_MAX_VALUE = AnalogIn::ADC_MAX_VALUE;
};

/// Alias for an 8-channel multiplexer (3 address pins).
using AnalogInMux8 = AnalogInMux<3>;

/// Alias for a 16-channel multiplexer (4 address pins).
using AnalogInMux16 = AnalogInMux<4>;

} // namespace musin::hal

#endif // MUSIN_HAL_ANALOG_IN_H
