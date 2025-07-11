#include "musin/hal/analog_mux_scanner.h"
#include "musin/hal/adc_defs.h" // For set_mux_address and pin_to_adc_channel

extern "C" {
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/assert.h"
}

namespace musin::hal {

AnalogMuxScanner::AnalogMuxScanner(uint32_t adc_pin,
                                   const etl::array<uint32_t, 4> &address_pins,
                                   uint32_t scan_interval_us, uint32_t settle_time_us)
    : _adc_pin(adc_pin), _adc_channel(pin_to_adc_channel(adc_pin)), _address_pins(address_pins),
      _scan_interval_us(scan_interval_us), _settle_time_us(settle_time_us),
      _last_scan_time(nil_time), _initialized(false) {
  _raw_values.fill(0);
}

void AnalogMuxScanner::init() {
  if (_initialized) {
    return;
  }

  adc_init();
  adc_gpio_init(_adc_pin);

  for (uint32_t pin : _address_pins) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
  }

  _last_scan_time = get_absolute_time();
  _initialized = true;
}

bool AnalogMuxScanner::scan() {
  if (!_initialized) {
    return false;
  }

  absolute_time_t now = get_absolute_time();
  if (absolute_time_diff_us(_last_scan_time, now) < _scan_interval_us) {
    return false;
  }

  _last_scan_time = now;
  perform_scan();
  return true;
}

uint16_t AnalogMuxScanner::get_raw_value(uint8_t channel) const {
  hard_assert(channel < NUM_CHANNELS);
  return _raw_values[channel];
}

void AnalogMuxScanner::perform_scan() {
  adc_select_input(_adc_channel);
  for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
    set_mux_address<etl::array<uint32_t, 4>>(_address_pins, i);
    if (_settle_time_us > 0) {
      sleep_us(_settle_time_us);
    }
    _raw_values[i] = adc_read();
  }
}

} // namespace musin::hal
