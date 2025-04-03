#ifndef MUSIN_HAL_ANALOG_IN_H
#define MUSIN_HAL_ANALOG_IN_H

#include <cstdint>
#include <vector>
// Standard types are sufficient for the interface definition.
// Platform-specific SDK headers are now included in the .cpp file.

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
  // ADC reference voltage (typically 3.3V for RP2040)
  static constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;
  // Maximum raw ADC value (12-bit)
  static constexpr float ADC_MAX_VALUE = 4095.0f; // Maximum raw ADC value (12-bit)


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

  // --- Helper Functions ---
  /**
   * @brief Sets the state of multiple GPIO pins based on a binary value.
   * Used to control multiplexer address lines.
   * @param address_pins Vector of GPIO pin numbers (index 0 = LSB).
   * @param address_value The address value to set.
   */
  static void set_mux_address(const std::vector<uint>& address_pins, uint8_t address_value);

};


/**
 * @brief Interface for reading an analog input pin via an 8-channel multiplexer (e.g., 74HC4051).
 * Requires 3 address pins.
 */
class AnalogInMux8 {
public:
  /**
   * @brief Construct an AnalogInMux8 instance.
   *
   * @param adc_pin The GPIO pin connected to the ADC input (must be ADC capable: 26, 27, 28).
   * @param address_pins Vector containing the 3 GPIO pin numbers for the mux address lines (index 0=LSB/S0, 1=S1, 2=MSB/S2).
   * @param channel_address The specific channel (0-7) on the multiplexer for this input.
   * @param address_settle_time_us Microseconds to wait after setting address pins before reading ADC.
   */
  AnalogInMux8(uint adc_pin,
                 const std::vector<uint>& address_pins,
                 uint8_t channel_address,
                 uint32_t address_settle_time_us = 5);

  AnalogInMux8(const AnalogInMux8&) = delete;
  AnalogInMux8& operator=(const AnalogInMux8&) = delete;

  /**
   * @brief Initialize the ADC, ADC GPIO pin, and address GPIO pins.
   * Must be called once before reading.
   */
  void init();

  /**
   * @brief Read the analog value from the configured mux channel (scaled to 16-bit).
   * Sets the mux address, waits briefly, then reads the ADC.
   * @return The 16-bit representation of the analog reading (0-65520). Returns 0 if not initialized.
   */
  std::uint16_t read() const;

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
  const uint _adc_pin;
  const uint _adc_channel;
  const std::vector<uint> _address_pins;
  const uint8_t _channel_address;
  const uint32_t _address_settle_time_us;
  bool _initialized = false;

  static constexpr float ADC_REFERENCE_VOLTAGE = AnalogIn::ADC_REFERENCE_VOLTAGE;
  static constexpr float ADC_MAX_VALUE = AnalogIn::ADC_MAX_VALUE;
};


/**
 * @brief Interface for reading an analog input pin via a 16-channel multiplexer (e.g., 74HC4067).
 * Requires 4 address pins.
 */
class AnalogInMux16 {
public:
  /**
   * @brief Construct an AnalogInMux16 instance.
   *
   * @param adc_pin The GPIO pin connected to the ADC input (must be ADC capable: 26, 27, 28).
   * @param address_pins Vector containing the 4 GPIO pin numbers for the mux address lines (index 0=LSB/S0, ..., 3=MSB/S3).
   * @param channel_address The specific channel (0-15) on the multiplexer for this input.
   * @param address_settle_time_us Microseconds to wait after setting address pins before reading ADC.
   */
  AnalogInMux16(uint adc_pin,
                  const std::vector<uint>& address_pins,
                  uint8_t channel_address,
                  uint32_t address_settle_time_us = 5); // Default 5us settle time

  // Prevent copying and assignment
  AnalogInMux16(const AnalogInMux16&) = delete;
  AnalogInMux16& operator=(const AnalogInMux16&) = delete;

  /**
   * @brief Initialize the ADC, ADC GPIO pin, and address GPIO pins.
   * Must be called once before reading.
   */
  void init();

  /**
   * @brief Read the analog value from the configured mux channel (scaled to 16-bit).
   * Sets the mux address, waits briefly, then reads the ADC.
   * @return The 16-bit representation of the analog reading (0-65520). Returns 0 if not initialized.
   */
  std::uint16_t read() const;

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
  const uint _adc_pin;
  const uint _adc_channel;
  const std::vector<uint> _address_pins;
  const uint8_t _channel_address;
  const uint32_t _address_settle_time_us;
  bool _initialized = false;

  static constexpr float ADC_REFERENCE_VOLTAGE = AnalogIn::ADC_REFERENCE_VOLTAGE;
  static constexpr float ADC_MAX_VALUE = AnalogIn::ADC_MAX_VALUE;
};


} // namespace Musin::HAL

#endif // MUSIN_HAL_ANALOG_IN_H
