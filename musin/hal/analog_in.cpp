#include "analog_in.h"

// Wrap C SDK headers
extern "C" {
#include "hardware/gpio.h" // Although included via adc.h, good practice to be explicit if using gpio functions directly
}

namespace Musin::HAL {

// --- Static Helper Implementation ---

uint AnalogIn::pin_to_adc_channel(uint pin) {
  // RP2040 specific: GPIO 26=ADC0, 27=ADC1, 28=ADC2, 29=ADC3 (Temp Sensor)
  hard_assert(pin >= 26 && pin <= 29); // Ensure valid ADC pin
  return pin - 26;
}

// --- Constructor Implementation ---

AnalogIn::AnalogIn(uint pin, bool enable_temp_sensor) :
  _pin(pin),
  _adc_channel(pin_to_adc_channel(pin)), // Derive channel from pin
  _enable_temp_sensor(enable_temp_sensor && (_adc_channel == 3)), // Only enable if channel is 3 (pin 29)
  _initialized(false)
{}

// --- Public Method Implementations ---

void AnalogIn::init() {
  if (_initialized) {
    return; // Already initialized
  }

  // Initialize ADC system if not already done (safe to call multiple times)
  adc_init();

  // Initialize the specific GPIO pin for ADC use
  adc_gpio_init(_pin);

  // Enable temperature sensor if requested and applicable
  if (_enable_temp_sensor) {
    adc_set_temp_sensor_enabled(true);
  }

  _initialized = true;
}

std::uint16_t AnalogIn::read() const {
  if (!_initialized) {
    // Or consider asserting: hard_assert(_initialized);
    return 0;
  }

  // Select the ADC channel for this pin
  adc_select_input(_adc_channel);

  // Perform the conversion
  std::uint16_t result12bit = adc_read();

  // Scale 12-bit result (0-4095) to 16-bit (0-65520)
  // Left shift by 4 is equivalent to multiplying by 16.
  // This maps the 12-bit range to the upper part of the 16-bit range.
  std::uint16_t result16bit = result12bit << 4;

  return result16bit;
}

std::uint16_t AnalogIn::read_raw() const {
   if (!_initialized) {
    // Or consider asserting: hard_assert(_initialized);
    return 0;
  }
   // Select the ADC channel for this pin
  adc_select_input(_adc_channel);

  // Perform the conversion and return the raw 12-bit value
  return adc_read();
}

} // namespace Musin::HAL
