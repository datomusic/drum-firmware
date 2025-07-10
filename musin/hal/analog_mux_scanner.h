#ifndef MUSIN_HAL_ANALOG_MUX_SCANNER_H
#define MUSIN_HAL_ANALOG_MUX_SCANNER_H

#include "etl/array.h"
#include "pico/time.h"
#include <cstdint>

namespace musin::hal {

class AnalogMuxScanner {
public:
  static constexpr size_t NUM_CHANNELS = 16;

  AnalogMuxScanner(uint32_t adc_pin,
                   const etl::array<uint32_t, 4> &address_pins,
                   uint32_t scan_interval_us = 1000,
                   uint32_t settle_time_us = 5);

  void init();
  bool scan();

  uint16_t get_raw_value(uint8_t channel) const;

private:
  void perform_scan();

  const uint32_t _adc_pin;
  const uint32_t _adc_channel;
  const etl::array<uint32_t, 4> _address_pins;
  const uint32_t _scan_interval_us;
  const uint32_t _settle_time_us;

  etl::array<uint16_t, NUM_CHANNELS> _raw_values;
  absolute_time_t _last_scan_time;
  bool _initialized;
};

} // namespace musin::hal

#endif // MUSIN_HAL_ANALOG_MUX_SCANNER_H
