#ifndef MUSIN_UI_ANALOG_CONTROL_H
#define MUSIN_UI_ANALOG_CONTROL_H

#include "musin/hal/adc_defs.h"
#include "musin/observer.h"
#include "musin/ui/adaptive_filter.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <cmath>

namespace musin::ui {

struct AnalogControlEvent {
  uint16_t control_id;
  float value;
  uint16_t raw_value;
};

template <auto *...Observers> class AnalogControl {
public:
  explicit AnalogControl(uint16_t control_id, bool invert = false, bool use_filter = true,
                         float threshold = 0.005f);

  void init();
  bool update(uint16_t raw_value);

  float get_value() const;
  uint16_t get_raw_value() const {
    return _current_raw;
  }
  uint16_t get_id() const {
    return _id;
  }
  void set_threshold(float threshold) {
    _threshold = threshold;
  }

private:
  uint16_t _id;
  bool _invert_mapping;

  std::optional<AdaptiveFilter> _filter; // Filter is now optional

  uint16_t _current_raw = 0;
  float _threshold;

  float _last_notified_value = -1.0f;
};

// --- Implementation ---

template <auto *...Observers>
AnalogControl<Observers...>::AnalogControl(uint16_t control_id, bool invert, bool use_filter,
                                           float threshold)
    : _id(control_id), _invert_mapping(invert), _threshold(threshold) {
  if (use_filter) {
    _filter.emplace();
  }
}

template <auto *...Observers> void AnalogControl<Observers...>::init() {
  _last_notified_value = -1.0f;
  _current_raw = 0;
  if (_filter) {
    _filter->update(0.0f); // Initialize filter value
  }
}

template <auto *...Observers> float AnalogControl<Observers...>::get_value() const {
  if (_filter) {
    return _filter->get_value();
  }
  // If no filter, we don't have a persistent value, this might need rethinking
  // For now, return a raw normalized value if requested directly.
  float raw_normalized = static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  return _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;
}

template <auto *...Observers> bool AnalogControl<Observers...>::update(uint16_t raw_value) {
  _current_raw = raw_value;
  float raw_normalized = static_cast<float>(_current_raw) / musin::hal::ADC_MAX_VALUE;
  float current_value = _invert_mapping ? (1.0f - raw_normalized) : raw_normalized;

  float value_to_check = current_value;

  if (_filter) {
    _filter->update(current_value);
    value_to_check = _filter->get_value();
  }

  if (std::abs(value_to_check - _last_notified_value) > _threshold) {
    musin::observable<Observers...>::notify_observers(
        AnalogControlEvent{_id, value_to_check, _current_raw});
    _last_notified_value = value_to_check;
    return true;
  }
  return false;
}

} // namespace musin::ui

#endif // MUSIN_UI_ANALOG_CONTROL_H