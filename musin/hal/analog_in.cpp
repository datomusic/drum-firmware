#include "analog_in.h"

// Include necessary Pico SDK headers for implementation
extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/assert.h"
#include "pico/time.h"
}

namespace Musin::HAL {

// --- Static Helper Implementation (AnalogIn) ---

// --- Static Helper Implementation (Shared) ---

// Defined in header, implementation here
void AnalogIn::set_mux_address(const std::vector<uint>& address_pins, uint8_t address_value) {
    hard_assert(address_pins.size() <= 8); // Max 8 address pins reasonable for uint8_t value
    for (size_t i = 0; i < address_pins.size(); ++i) {
        // Set pin state based on the corresponding bit in address_value
        // (address_value >> i) & 1 extracts the i-th bit (LSB = bit 0)
        gpio_put(address_pins[i], (address_value >> i) & 1);
    }
}


// --- AnalogIn Implementation ---

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

float AnalogIn::read_voltage() const {
  if (!_initialized) {
    return 0.0f;
  }
  // Read the raw 12-bit value
  std::uint16_t raw_value = read_raw();

  // Convert the raw 12-bit value to voltage
  // voltage = (raw_value / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE
  return (static_cast<float>(raw_value) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}


// --- AnalogInMux8 Implementation ---

AnalogInMux8::AnalogInMux8(uint adc_pin,
                           const std::vector<uint>& address_pins,
                           uint8_t channel_address,
                           uint32_t address_settle_time_us) :
  _adc_pin(adc_pin),
  _adc_channel(AnalogIn::pin_to_adc_channel(adc_pin)), // Reuse helper
  _address_pins(address_pins),
  _channel_address(channel_address),
  _address_settle_time_us(address_settle_time_us),
  _initialized(false)
{
    hard_assert(address_pins.size() == 3); // Mux8 requires exactly 3 address pins
    hard_assert(channel_address < 8);      // Valid channel range 0-7
}

void AnalogInMux8::init() {
    if (_initialized) {
        return;
    }

    // Init ADC system and the specific ADC pin
    adc_init();
    adc_gpio_init(_adc_pin);

    // Init address pins as outputs
    for (uint pin : _address_pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0); // Default to address 0 initially
    }

    _initialized = true;
}

std::uint16_t AnalogInMux8::read_raw() const {
    if (!_initialized) {
        return 0;
    }

    // Set the multiplexer address
    AnalogIn::set_mux_address(_address_pins, _channel_address);

    // Wait for the multiplexer and signal to settle (optional but recommended)
    if (_address_settle_time_us > 0) {
        busy_wait_us(_address_settle_time_us);
    }

    // Select the ADC input channel (connected to the mux output)
    adc_select_input(_adc_channel);

    // Perform the conversion
    return adc_read(); // Returns 12-bit value
}

std::uint16_t AnalogInMux8::read() const {
    std::uint16_t raw = read_raw();
    // Scale 12-bit to 16-bit
    return raw << 4;
}

float AnalogInMux8::read_voltage() const {
    std::uint16_t raw = read_raw();
    return (static_cast<float>(raw) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}


// --- AnalogInMux16 Implementation ---

AnalogInMux16::AnalogInMux16(uint adc_pin,
                             const std::vector<uint>& address_pins,
                             uint8_t channel_address,
                             uint32_t address_settle_time_us) :
  _adc_pin(adc_pin),
  _adc_channel(AnalogIn::pin_to_adc_channel(adc_pin)), // Reuse helper
  _address_pins(address_pins),
  _channel_address(channel_address),
  _address_settle_time_us(address_settle_time_us),
  _initialized(false)
{
    hard_assert(address_pins.size() == 4); // Mux16 requires exactly 4 address pins
    hard_assert(channel_address < 16);     // Valid channel range 0-15
}

void AnalogInMux16::init() {
    if (_initialized) {
        return;
    }

    // Init ADC system and the specific ADC pin
    adc_init();
    adc_gpio_init(_adc_pin);

    // Init address pins as outputs
    for (uint pin : _address_pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0); // Default to address 0 initially
    }

    _initialized = true;
}

std::uint16_t AnalogInMux16::read_raw() const {
    if (!_initialized) {
        return 0;
    }

    // Set the multiplexer address
    AnalogIn::set_mux_address(_address_pins, _channel_address);

    // Wait for the multiplexer and signal to settle
    if (_address_settle_time_us > 0) {
        busy_wait_us(_address_settle_time_us);
    }

    // Select the ADC input channel (connected to the mux output)
    adc_select_input(_adc_channel);

    // Perform the conversion
    return adc_read(); // Returns 12-bit value
}

std::uint16_t AnalogInMux16::read() const {
    std::uint16_t raw = read_raw();
    // Scale 12-bit to 16-bit
    return raw << 4;
}

float AnalogInMux16::read_voltage() const {
    std::uint16_t raw = read_raw();
    return (static_cast<float>(raw) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}


} // namespace Musin::HAL
