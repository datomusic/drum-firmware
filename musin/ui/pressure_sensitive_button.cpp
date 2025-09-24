#include "pressure_sensitive_button.h"
#include <algorithm>

namespace musin::ui {

PressureSensitiveButton::PressureSensitiveButton(
    uint16_t button_id, const PressureSensitiveButtonConfig &config)
    : button_id_(button_id), config_(config) {
}

void PressureSensitiveButton::update(float value, absolute_time_t now) {
  PressureState next_state = calculate_next_state(value);

  if (next_state != current_state_ && is_debounce_satisfied(now)) {
    PressureState previous_state = current_state_;
    current_state_ = next_state;
    last_transition_time_ = now;

    this->notify_observers(PressureSensitiveButtonEvent{
        button_id_, current_state_, previous_state, value});
  }
}

bool PressureSensitiveButton::is_debounce_satisfied(absolute_time_t now) const {
  if (is_nil_time(last_transition_time_)) {
    return true;
  }

  const uint32_t debounce_us = config_.debounce_ms * 1000u;
  return absolute_time_diff_us(last_transition_time_, now) >=
         static_cast<int64_t>(debounce_us);
}

PressureState PressureSensitiveButton::calculate_next_state(float value) const {
  switch (current_state_) {
  case PressureState::Released:
    if (value >= config_.hard_press_threshold) {
      return PressureState::HardPress;
    } else if (value >= config_.light_press_threshold) {
      return PressureState::LightPress;
    }
    return PressureState::Released;

  case PressureState::LightPress:
    if (value >= config_.hard_press_threshold) {
      return PressureState::HardPress;
    } else if (value <= config_.light_release_threshold) {
      return PressureState::Released;
    }
    return PressureState::LightPress;

  case PressureState::HardPress:
    if (value <= config_.light_release_threshold) {
      return PressureState::Released;
    } else if (value <= config_.hard_release_threshold) {
      return PressureState::LightPress;
    }
    return PressureState::HardPress;
  }

  return current_state_;
}

} // namespace musin::ui