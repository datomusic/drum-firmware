#include "analog_control.h"
#include <cmath> // Required for std::abs
#include <new>   // Required for placement new

namespace Musin::UI {

AnalogControl::AnalogControl(uint32_t adc_pin, bool invert, float threshold)
    : _id(adc_pin), _invert_mapping(invert), _threshold(threshold), _input_type(InputType::Direct),
      _analog_in(adc_pin) {
}

AnalogControl::AnalogControl(uint32_t adc_pin, const std::array<std::uint32_t, 3> &mux_address_pins,
                             uint8_t mux_channel, bool invert, float threshold)
    : _id((static_cast<uint16_t>(mux_channel) << 8) | adc_pin), _invert_mapping(invert),
      _threshold(threshold), _input_type(InputType::Mux8) {
  new (&_mux8) Musin::HAL::AnalogInMux8(adc_pin, mux_address_pins, mux_channel);
}

AnalogControl::AnalogControl(uint32_t adc_pin, const std::array<std::uint32_t, 4> &mux_address_pins,
                             uint8_t mux_channel, bool invert, float threshold)
    : _id((static_cast<uint16_t>(mux_channel) << 8) | adc_pin), _invert_mapping(invert),
      _threshold(threshold), _input_type(InputType::Mux16) {
  new (&_mux16) Musin::HAL::AnalogInMux16(adc_pin, mux_address_pins, mux_channel);
}

void AnalogControl::init() {
  switch (_input_type) {
  case InputType::Direct:
    _analog_in.init();
    break;
  case InputType::Mux8:
    _mux8.init();
    break;
  case InputType::Mux16:
    _mux16.init();
    break;
  }
  read_input();
  _last_notified_value = _current_value;
}

void AnalogControl::read_input() {
  float raw_normalized = 0.0f;
  switch (_input_type) {
  case InputType::Direct:
    raw_normalized = _analog_in.read();
    _current_raw = _analog_in.read_raw();
    break;
  case InputType::Mux8:
    raw_normalized = _mux8.read();
    _current_raw = _mux8.read_raw();
    break;
  case InputType::Mux16:
    raw_normalized = _mux16.read();
    _current_raw = _mux16.read_raw();
    break;
  }

  // Apply inversion if requested
  float value_to_filter = _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;

  _filtered_value = (_filter_alpha * value_to_filter) + ((1.0f - _filter_alpha) * _filtered_value);
  _current_value = _filtered_value;
}

bool AnalogControl::update() {
  read_input();
  if (std::abs(_current_value - _last_notified_value) > _threshold) {
    this->notify_observers(AnalogControlEvent{_id, _current_value, _current_raw});
    _last_notified_value = _current_value;
    return true;
  }
  return false;
}

} // namespace Musin::UI
