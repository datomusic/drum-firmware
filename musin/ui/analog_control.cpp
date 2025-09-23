#include "analog_control.h"
#include "musin/hal/adc_defs.h" // For ADC_MAX_VALUE
#include <cmath>

namespace musin::ui {

AnalogControl::AnalogControl(uint16_t control_id, bool invert, bool use_filter,
                             float threshold)
    : _id(control_id), _invert_mapping(invert), _threshold(threshold) {
  if (use_filter) {
    _filter.emplace();
  }
}

void AnalogControl::init(uint16_t initial_raw_value) {
  _current_raw = initial_raw_value;
  float raw_normalized =
      static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  float initial_value =
      _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;

  if (_filter) {
    // Initialize the filter with the first reading to prevent it from starting
    // at 0 and ramping up.
    _filter->update(initial_value);
    _last_notified_value = _filter->get_value();
  } else {
    _last_notified_value = initial_value;
  }
}

float AnalogControl::get_value() const {
  if (_filter) {
    return _filter->get_value();
  }
  // If no filter, we don't have a persistent value, this might need rethinking
  // For now, return a raw normalized value if requested directly.
  float raw_normalized =
      static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  return _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;
}

bool AnalogControl::update(uint16_t raw_value) {
  _current_raw = raw_value;
  float raw_normalized =
      static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  float current_value =
      _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;

  float value_to_check = current_value;

  if (_filter) {
    _filter->update(current_value);
    value_to_check = _filter->get_value();
  }

  if (std::abs(value_to_check - _last_notified_value) > _threshold) {
    // Calculate velocity of change for hard press detection
    _last_change_velocity = std::abs(value_to_check - _last_notified_value);

    this->notify_observers(
        AnalogControlEvent{_id, value_to_check, _current_raw});
    _last_notified_value = value_to_check;
    return true;
  }
  return false;
}

bool AnalogControl::is_hard_pressed(float threshold) const {
  return get_value() >= threshold && _last_change_velocity > 0.1f;
}

float AnalogControl::get_press_velocity() const {
  return _last_change_velocity;
}

} // namespace musin::ui
