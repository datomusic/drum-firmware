#include "musin/ui/adaptive_filter.h"
#include <algorithm> // For std::clamp
#include <cmath>     // For std::fabs and std::lerp

namespace musin::ui {

AdaptiveFilter::AdaptiveFilter(float min_alpha, float max_alpha, float sensitivity)
    : _current_value(0.0f), _min_alpha(min_alpha), _max_alpha(max_alpha), _sensitivity(sensitivity) {}

void AdaptiveFilter::update(float new_value) {
  float difference = std::fabs(new_value - _current_value);

  // Dynamically adjust alpha based on the rate of change
  float alpha = _min_alpha + (_max_alpha - _min_alpha) * (1.0f - std::exp(-_sensitivity * difference));
  alpha = std::clamp(alpha, _min_alpha, _max_alpha);

  // Apply the low-pass filter
  _current_value = std::lerp(_current_value, new_value, alpha);
}

float AdaptiveFilter::get_value() const {
  return _current_value;
}

} // namespace musin::ui
