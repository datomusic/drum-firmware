#ifndef MUSIN_UI_ANALOG_CONTROL_H
#define MUSIN_UI_ANALOG_CONTROL_H

#include "etl/observer.h"
#include "musin/hal/analog_mux_scanner.h" // New include
#include <algorithm>
#include <cstdint>

namespace musin::ui {

struct AnalogControlEvent {
  uint16_t control_id;
  float value;
  uint16_t raw_value;
};

class AnalogControl : public etl::observable<etl::observer<AnalogControlEvent>, 4> {
public:
  explicit AnalogControl(uint16_t control_id, bool invert = false, float threshold = 0.005f);

  void init();
  bool update(uint16_t raw_value);

  float get_value() const {
    return _current_value;
  }
  uint16_t get_raw_value() const {
    return _current_raw;
  }
  uint16_t get_id() const {
    return _id;
  }
  void set_filter_coefficient(float alpha) {
    _filter_alpha = std::clamp(alpha, 0.0f, 1.0f);
  }
  void set_threshold(float threshold) {
    _threshold = threshold;
  }

private:
  uint16_t _id;
  bool _invert_mapping;

  float _current_value = 0.0f;
  float _filtered_value = 0.0f;
  uint16_t _current_raw = 0;
  float _threshold;
  float _filter_alpha = 0.3f;

  float _last_notified_value = -1.0f;
};

} // namespace musin::ui

#endif // MUSIN_UI_ANALOG_CONTROL_H
