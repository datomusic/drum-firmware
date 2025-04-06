#include "analog_in.h"

// Include necessary Pico SDK headers for implementation
extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/assert.h"
#include "pico/time.h"
}
#include <vector> // Keep for the original set_mux_address if needed elsewhere, or remove
#include <array>  // Include for std::array

namespace Musin::HAL {

// --- Free Helper Function Implementations ---

// Template implementation for set_mux_address
template <typename Container>
void set_mux_address(const Container& address_pins, uint8_t address_value) {
    // No hard_assert on size here, rely on the caller or class template constraints
    for (size_t i = 0; i < address_pins.size(); ++i) {
        // Ensure we don't read past the end if container is smaller than expected bits
        if (i >= sizeof(address_value) * 8) break;
        gpio_put(address_pins[i], (address_value >> i) & 1);
    }
}

// --- Explicit Instantiation for common containers ---
// Explicitly instantiate the template function for the array sizes we use.
template void set_mux_address<std::array<std::uint32_t, 3>>(const std::array<std::uint32_t, 3>&, uint8_t);
template void set_mux_address<std::array<std::uint32_t, 4>>(const std::array<std::uint32_t, 4>&, uint8_t);
// If you still need the vector version elsewhere, keep its definition and add:
// template void set_mux_address<std::vector<std::uint32_t>>(const std::vector<std::uint32_t>&, uint8_t);
// Otherwise, the old vector implementation can be removed.


// --- Free Helper Function Implementations (Continued) ---

std::uint32_t pin_to_adc_channel(std::uint32_t pin) {
  // RP2040 specific: GPIO 26=ADC0, 27=ADC1, 28=ADC2, 29=ADC3 (Temp Sensor)
  hard_assert(pin >= 26 && pin <= 29); // Ensure valid ADC pin
  return pin - 26;
}

// --- Constructor Implementation ---

AnalogIn::AnalogIn(std::uint32_t pin, bool enable_temp_sensor) :
  _pin(pin),
  _adc_channel(Musin::HAL::pin_to_adc_channel(pin)), // Call free function
  _enable_temp_sensor(enable_temp_sensor && (_adc_channel == 3)),
  _initialized(false)
{}

// --- Public Method Implementations ---

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

std::uint16_t AnalogIn::read() const {
  if (!_initialized) {
    return 0;
  }

  adc_select_input(_adc_channel);
  std::uint16_t result12bit = adc_read();
  std::uint16_t result16bit = result12bit << 4; // Scale 12-bit to 16-bit

  return result16bit;
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
} // <-- Added missing closing brace


// --- Template Implementation for AnalogInMux<NumAddressPins> ---

template <size_t NumAddressPins>
AnalogInMux<NumAddressPins>::AnalogInMux(std::uint32_t adc_pin,
                                         const std::array<std::uint32_t, NumAddressPins>& address_pins,
                                         uint8_t channel_address,
                                         std::uint32_t address_settle_time_us) :
  _adc_pin(adc_pin),
  _adc_channel(Musin::HAL::pin_to_adc_channel(adc_pin)),
  _address_pins(address_pins), // Directly initialize std::array member
  _channel_address(channel_address),
  _address_settle_time_us(address_settle_time_us),
  _initialized(false)
{
    // Runtime check for valid channel address for this mux configuration
    hard_assert(channel_address < MAX_CHANNELS);
    // The check for the correct number of address pins is handled by std::array
    // and the template parameter itself.
}

template <size_t NumAddressPins>
void AnalogInMux<NumAddressPins>::init() {
    if (_initialized) {
        return;
    }

    adc_init();
    adc_gpio_init(_adc_pin);

    // Init address pins as outputs
    for (std::uint32_t pin : _address_pins) { // Iterate over std::array
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0); // Initialize to 0
    }

    _initialized = true;
}

template <size_t NumAddressPins>
std::uint16_t AnalogInMux<NumAddressPins>::read_raw() const {
    if (!_initialized) {
        return 0;
    }

    // Call the templated helper function, explicitly providing the template argument
    Musin::HAL::set_mux_address<std::array<std::uint32_t, NumAddressPins>>(_address_pins, _channel_address);

    // Wait for the multiplexer and signal to settle
    if (_address_settle_time_us > 0) {
        busy_wait_us(_address_settle_time_us);
    }

    adc_select_input(_adc_channel);
    return adc_read();
}

template <size_t NumAddressPins>
std::uint16_t AnalogInMux<NumAddressPins>::read() const {
    std::uint16_t raw = read_raw();
    return raw << 4; // Scale 12-bit to 16-bit
}

template <size_t NumAddressPins>
float AnalogInMux<NumAddressPins>::read_voltage() const {
    std::uint16_t raw = read_raw();
    return (static_cast<float>(raw) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}

// --- Explicit Template Instantiation ---
// You MUST explicitly instantiate the template class versions you intend to use
// because the implementation is in the .cpp file.
template class AnalogInMux<3>; // For AnalogInMux8 alias
template class AnalogInMux<4>; // For AnalogInMux16 alias


} // namespace Musin::HAL
