#include "analog_in.h"

// Include necessary Pico SDK headers for implementation
extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/assert.h"
#include "pico/time.h"
}
#include <array>
#include <vector>

namespace Musin::HAL {

template <typename Container>
void set_mux_address(const Container &address_pins, uint8_t address_value) {
  for (size_t i = 0; i < address_pins.size(); ++i) {
    if (i >= sizeof(address_value) * 8)
      break; // Avoid shifting beyond the value's bits
    gpio_put(address_pins[i], (address_value >> i) & 1);
  }
}

// Explicit Instantiation
template void set_mux_address<std::array<std::uint32_t, 3>>(const std::array<std::uint32_t, 3> &,
                                                            uint8_t);
template void set_mux_address<std::array<std::uint32_t, 4>>(const std::array<std::uint32_t, 4> &,
                                                            uint8_t);
// template void set_mux_address<std::vector<std::uint32_t>>(const std::vector<std::uint32_t>&,
// uint8_t);

std::uint32_t pin_to_adc_channel(std::uint32_t pin) {
  hard_assert(pin >= 26 && pin <= 29); // RP2040: GPIO 26=ADC0, 27=ADC1, 28=ADC2, 29=ADC3/Temp
  return pin - 26;
}

// --- AnalogIn ---

AnalogIn::AnalogIn(std::uint32_t pin, bool enable_temp_sensor)
    : _pin(pin), _adc_channel(Musin::HAL::pin_to_adc_channel(pin)),
      _enable_temp_sensor(enable_temp_sensor && (_adc_channel == 3)), // Temp sensor is ADC3
      _initialized(false) {
}

void AnalogIn::init() {
  if (_initialized) {
    return;
  }

  adc_init();
  adc_gpio_init(_pin);

  if (_enable_temp_sensor) {
    adc_set_temp_sensor_enabled(true);
  }

  _initialized = true;
}

float AnalogIn::read() const {
  if (!_initialized) {
    return 0.0f;
  }
  std::uint16_t raw_value = read_raw();
  // Normalize the 12-bit raw value to a float between 0.0 and 1.0
  return static_cast<float>(raw_value) / ADC_MAX_VALUE;
}

std::uint16_t AnalogIn::read_raw() const {
  if (!_initialized) {
    return 0;
  }
  adc_select_input(_adc_channel);
  return adc_read();
}

float AnalogIn::read_voltage() const {
  if (!_initialized) {
    return 0.0f;
  }
  std::uint16_t raw_value = read_raw();
  return (static_cast<float>(raw_value) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}

// --- AnalogInMux<NumAddressPins> ---

template <size_t NumAddressPins>
AnalogInMux<NumAddressPins>::AnalogInMux(
    std::uint32_t adc_pin, const std::array<std::uint32_t, NumAddressPins> &address_pins,
    uint8_t channel_address, std::uint32_t address_settle_time_us)
    : _adc_pin(adc_pin), _adc_channel(Musin::HAL::pin_to_adc_channel(adc_pin)),
      _address_pins(address_pins), _channel_address(channel_address),
      _address_settle_time_us(address_settle_time_us), _initialized(false) {
  hard_assert(channel_address < MAX_CHANNELS);
}

template <size_t NumAddressPins> void AnalogInMux<NumAddressPins>::init() {
  if (_initialized) {
    return;
  }

  adc_init();
  adc_gpio_init(_adc_pin);

  for (std::uint32_t pin : _address_pins) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
  }

  _initialized = true;
}

template <size_t NumAddressPins> std::uint16_t AnalogInMux<NumAddressPins>::read_raw() const {
  if (!_initialized) {
    return 0;
  }

  Musin::HAL::set_mux_address<std::array<std::uint32_t, NumAddressPins>>(_address_pins,
                                                                         _channel_address);

  if (_address_settle_time_us > 0) {
    busy_wait_us(_address_settle_time_us);
  }

  adc_select_input(_adc_channel);
  return adc_read();
}

template <size_t NumAddressPins> float AnalogInMux<NumAddressPins>::read() const {
  std::uint16_t raw = read_raw();
  // Normalize the 12-bit raw value to a float between 0.0 and 1.0
  return static_cast<float>(raw) / ADC_MAX_VALUE;
}

template <size_t NumAddressPins> float AnalogInMux<NumAddressPins>::read_voltage() const {
  std::uint16_t raw = read_raw();
  return (static_cast<float>(raw) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}

// Explicit Template Instantiation
template class AnalogInMux<3>; // AnalogInMux8
template class AnalogInMux<4>; // AnalogInMux16

} // namespace Musin::HAL
