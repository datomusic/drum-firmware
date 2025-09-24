#ifndef MUSIN_UI_PRESSURE_SENSITIVE_BUTTON_H
#define MUSIN_UI_PRESSURE_SENSITIVE_BUTTON_H

#include "etl/observer.h"
#include "pico/time.h"
#include <cstdint>

namespace musin::ui {

enum class PressureState : uint8_t {
  Released = 0,
  LightPress = 1,
  HardPress = 2
};

struct PressureSensitiveButtonEvent {
  uint16_t button_id;
  PressureState state;
  PressureState previous_state;
  float current_value;
};

struct PressureSensitiveButtonConfig {
  float light_press_threshold = 0.3f;
  float hard_press_threshold = 0.8f;
  float light_release_threshold = 0.25f; // Hysteresis
  float hard_release_threshold = 0.75f;  // Hysteresis
  uint32_t debounce_ms = 30;
};

class PressureSensitiveButton
    : public etl::observable<etl::observer<PressureSensitiveButtonEvent>, 4> {
public:
  explicit PressureSensitiveButton(
      uint16_t button_id, const PressureSensitiveButtonConfig &config = {});

  void update(float value, absolute_time_t now);

  PressureState get_state() const {
    return current_state_;
  }
  uint16_t get_id() const {
    return button_id_;
  }

  void set_config(const PressureSensitiveButtonConfig &config) {
    config_ = config;
  }

private:
  uint16_t button_id_;
  PressureSensitiveButtonConfig config_;
  PressureState current_state_ = PressureState::Released;
  absolute_time_t last_transition_time_ = nil_time;

  bool is_debounce_satisfied(absolute_time_t now) const;
  PressureState calculate_next_state(float value) const;
};

} // namespace musin::ui

#endif // MUSIN_UI_PRESSURE_SENSITIVE_BUTTON_H