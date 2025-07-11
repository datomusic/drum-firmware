#include "analog_control.h"
#include "musin/hal/adc_defs.h" // For ADC_MAX_VALUE
#include <cmath>

namespace musin::ui {

AnalogControl::AnalogControl(uint16_t control_id, bool invert, bool use_filter, float threshold)
    : _id(control_id), _invert_mapping(invert), _threshold(threshold) {
  if (use_filter) {
    _filter.emplace();
  }
}

void AnalogControl::init() {
  _last_notified_value = -1.0f;
  _current_raw = 0;
  if (_filter) {
    _filter->update(0.0f); // Initialize filter value
  }
}

float AnalogControl::get_value() const {
  if (_filter) {
    return _filter->get_value();
  }
  // If no filter, we don't have a persistent value, this might need rethinking
  // For now, return a raw normalized value if requested directly.
  float raw_normalized = static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  return _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;
}

bool AnalogControl::update(uint16_t raw_value) {
  _current_raw = raw_value;
  float raw_normalized = static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  float current_value = _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;

  float value_to_check = current_value;

  if (_filter) {
    _filter->update(current_value);
    value_to_check = _filter->get_value();
  }

  if (std::abs(value_to_check - _last_notified_value) > _threshold) {
    this->notify_observers(AnalogControlEvent{_id, value_to_check, _current_raw});
    _last_notified_value = value_to_check;
    return true;
  }
  return false;
}

} // namespace musin::ui
