#include "analog_in.h"

// Include necessary Pico SDK headers for implementation
extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/assert.h"
#include "pico/time.h"
}

namespace Musin::HAL {

// --- Static Helper Implementation (Shared) ---

void AnalogIn::set_mux_address(const std::vector<uint>& address_pins, uint8_t address_value) {
    hard_assert(address_pins.size() <= 8);
    for (size_t i = 0; i < address_pins.size(); ++i) {
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
  _adc_channel(pin_to_adc_channel(pin)),
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
}


// --- AnalogInMux8 Implementation ---

AnalogInMux8::AnalogInMux8(uint adc_pin,
                           const std::vector<uint>& address_pins,
                           uint8_t channel_address,
                           uint32_t address_settle_time_us) :
  _adc_pin(adc_pin),
  _adc_channel(AnalogIn::pin_to_adc_channel(adc_pin)),
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

    adc_init();
    adc_gpio_init(_adc_pin);

    // Init address pins as outputs
    for (uint pin : _address_pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
    }

    _initialized = true;
}

std::uint16_t AnalogInMux8::read_raw() const {
    if (!_initialized) {
        return 0;
    }

    AnalogIn::set_mux_address(_address_pins, _channel_address);

    // Wait for the multiplexer and signal to settle (optional but recommended)
    if (_address_settle_time_us > 0) {
        busy_wait_us(_address_settle_time_us);
    }

    adc_select_input(_adc_channel);
    return adc_read();
}

std::uint16_t AnalogInMux8::read() const {
    std::uint16_t raw = read_raw();
    return raw << 4; // Scale 12-bit to 16-bit
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
  _adc_channel(AnalogIn::pin_to_adc_channel(adc_pin)),
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

    adc_init();
    adc_gpio_init(_adc_pin);

    // Init address pins as outputs
    for (uint pin : _address_pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
    }

    _initialized = true;
}

std::uint16_t AnalogInMux16::read_raw() const {
    if (!_initialized) {
        return 0;
    }

    AnalogIn::set_mux_address(_address_pins, _channel_address);

    // Wait for the multiplexer and signal to settle
    if (_address_settle_time_us > 0) {
        busy_wait_us(_address_settle_time_us);
    }

    adc_select_input(_adc_channel);
    return adc_read();
}

std::uint16_t AnalogInMux16::read() const {
    std::uint16_t raw = read_raw();
    return raw << 4; // Scale 12-bit to 16-bit
}

float AnalogInMux16::read_voltage() const {
    std::uint16_t raw = read_raw();
    return (static_cast<float>(raw) / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
}


} // namespace Musin::HAL
