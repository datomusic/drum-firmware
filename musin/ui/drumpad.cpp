#include "musin/ui/drumpad.h"
#include "drum/config.h"

namespace musin::ui {

Drumpad::Drumpad(uint8_t pad_id,
                 const drum::config::drumpad::DrumpadConfig &config)
    : _pad_id(pad_id), _noise_threshold(config.noise_threshold),
      _trigger_threshold(config.trigger_threshold),
      _high_pressure_threshold(
          config.high_pressure_threshold),
      _active_low(config.active_low),
      _debounce_time_us(config.debounce_time_us),
      _hold_time_us(config.hold_time_us),
      _max_velocity_time_us(config.max_velocity_time_us),
      _min_velocity_time_us(config.min_velocity_time_us) {
}

void Drumpad::init() {
  _current_state = DrumpadState::Idle;
  _current_retrigger_mode = RetriggerMode::Off;
  _last_adc_value = _active_low ? musin::hal::ADC_MAX_VALUE : 0;
  _state_transition_time = nil_time;
  _velocity_low_time = nil_time;
  _velocity_high_time = nil_time;
  _just_pressed = false;
  _just_released = false;
  _last_velocity = std::nullopt;
}

void Drumpad::update(uint16_t raw_adc_value) {
  absolute_time_t now = get_absolute_time();
  _just_pressed = false;
  _just_released = false;
  _last_velocity = std::nullopt;

  uint16_t value =
      _active_low ? musin::hal::ADC_MAX_VALUE - raw_adc_value : raw_adc_value;

  _last_adc_value = value;

  update_state_machine(value, now);
}

void Drumpad::update_state_machine(std::uint16_t current_adc_value,
                                   absolute_time_t now) {
  uint64_t time_in_state = absolute_time_diff_us(_state_transition_time, now);

  switch (_current_state) {
  case DrumpadState::Idle:
    if (current_adc_value >= _noise_threshold) {
      _current_state = DrumpadState::Rising;
      _current_retrigger_mode = RetriggerMode::Off;
      _state_transition_time = now;
      _velocity_low_time = now; // Start timing for velocity from here
      _velocity_high_time = nil_time;
    }
    break;

  case DrumpadState::Rising:
    if (current_adc_value >= _trigger_threshold) {
      _velocity_high_time = now;
      _current_state = DrumpadState::Peaking;
      _state_transition_time = now;

      uint64_t diff =
          absolute_time_diff_us(_velocity_low_time, _velocity_high_time);
      _last_velocity = calculate_velocity(diff);
      _just_pressed = true;
      notify_event(DrumpadEvent::Type::Press, _last_velocity,
                   current_adc_value);
    } else if (current_adc_value < _noise_threshold) {
      _current_state = DrumpadState::Idle;
      _state_transition_time = now;
    }
    break;

  case DrumpadState::Peaking:
    if (current_adc_value < _trigger_threshold) {
      _current_state = DrumpadState::Falling;
    }
    if (time_in_state >= _hold_time_us) {
      _current_state = DrumpadState::Holding;
      notify_event(DrumpadEvent::Type::Hold, std::nullopt, current_adc_value);
    } else if (current_adc_value < _noise_threshold) {
      _current_state = DrumpadState::DebouncingRelease;
      _current_retrigger_mode = RetriggerMode::Off;
      _state_transition_time = now;
    }
    break;

  case DrumpadState::Falling:
    if (current_adc_value < _noise_threshold) {
      _current_state = DrumpadState::DebouncingRelease;
      _current_retrigger_mode = RetriggerMode::Off;
      _state_transition_time = now;
    }
    break;

  case DrumpadState::Holding:
    if (current_adc_value >= _high_pressure_threshold) {
      _current_retrigger_mode = RetriggerMode::Double;
    } else if (current_adc_value >= _trigger_threshold) {
      _current_retrigger_mode = RetriggerMode::Single;
    } else {
      _current_retrigger_mode = RetriggerMode::Off;
    }

    if (current_adc_value < _trigger_threshold) {
      _current_state = DrumpadState::Falling;
    }
    break;

  case DrumpadState::DebouncingRelease:
    if (current_adc_value >= _noise_threshold) {
      _current_state = DrumpadState::Falling;
      _state_transition_time = now;
    } else if (time_in_state >= _debounce_time_us) {
      notify_event(DrumpadEvent::Type::Release, std::nullopt,
                   current_adc_value);
      _current_state = DrumpadState::Idle;
      _current_retrigger_mode = RetriggerMode::Off;
      _state_transition_time = now;
      _just_released = true;
      _last_adc_value = 0;
      _velocity_low_time = nil_time;
      _velocity_high_time = nil_time;
    }
    break;
  }
}

uint8_t Drumpad::calculate_velocity(uint64_t time_diff_us) const {
  if (time_diff_us <= _min_velocity_time_us) {
    return 127;
  }
  if (time_diff_us >= _max_velocity_time_us) {
    return 1;
  }

  uint64_t time_range = _max_velocity_time_us - _min_velocity_time_us;
  uint64_t adjusted_time = time_diff_us - _min_velocity_time_us;

  uint64_t velocity_scaled = 126ULL * (time_range - adjusted_time);
  uint8_t velocity = 1 + static_cast<uint8_t>(velocity_scaled / time_range);

  return velocity;
}

void Drumpad::notify_event(DrumpadEvent::Type type,
                           std::optional<uint8_t> velocity,
                           uint16_t raw_value) {
  DrumpadEvent event{.pad_index = _pad_id,
                     .type = type,
                     .velocity = velocity,
                     .raw_value = raw_value};
  this->notify_observers(event);
}

} // namespace musin::ui
