#include "analog_control.h"
#include "musin/hal/adc_defs.h" // For ADC_MAX_VALUE
#include <cmath>

namespace musin::ui {

AnalogControl::AnalogControl(uint16_t control_id, bool invert, float threshold)
    : _id(control_id), _invert_mapping(invert), _threshold(threshold) {
}

void AnalogControl::init() {
  _last_notified_value = -1.0f;
  _current_value = 0.0f;
  _filtered_value = 0.0f;
  _current_raw = 0;
}

bool AnalogControl::update(uint16_t raw_value) {
  _current_raw = raw_value;
  float raw_normalized = static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;

  float value_to_filter = _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;

  _filtered_value = (_filter_alpha * value_to_filter) + ((1.0f - _filter_alpha) * _filtered_value);
  _current_value = _filtered_value;

  if (std::abs(_current_value - _last_notified_value) > _threshold) {
    this->notify_observers(AnalogControlEvent{_id, _current_value, _current_raw});
    _last_notified_value = _current_value;
    return true;
  }
  return false;
}

} // namespace musin::ui
