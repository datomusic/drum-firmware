#ifndef MUSIN_UI_ADAPTIVE_FILTER_H
#define MUSIN_UI_ADAPTIVE_FILTER_H

#include <cmath>
#include <cstdint>

namespace musin::ui {

class AdaptiveFilter {
public:
  explicit AdaptiveFilter(float min_alpha = 0.1f, float max_alpha = 0.8f, float sensitivity = 2.0f);

  void update(float new_value);
  float get_value() const;

private:
  float _current_value;
  float _min_alpha;
  float _max_alpha;
  float _sensitivity;
};

} // namespace musin::ui

#endif // MUSIN_UI_ADAPTIVE_FILTER_H
