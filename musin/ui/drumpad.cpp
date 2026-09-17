#include "musin/ui/drumpad.h"

namespace musin::ui {

Drumpad::Drumpad(uint8_t pad_id, const DrumpadConfig &config)
    : _pad_id(pad_id), _noise_threshold(config.noise_threshold),
      _trigger_threshold(config.trigger_threshold),
      _high_pressure_threshold(config.high_pressure_threshold),
      _active_low(config.active_low),
      _debounce_time_us(config.debounce_time_us),
      _hold_time_us(config.hold_time_us),
      _max_velocity_time_us(config.max_velocity_time_us),
      _min_velocity_time_us(config.min_velocity_time_us),
      _pressure_hysteresis(config.pressure_hysteresis) {
}

void Drumpad::init() {
  _current_state = DrumpadState::Idle;
  _pressure_level = PressureLevel::None;
  _last_adc_value = _active_low ? musin::hal::ADC_MAX_VALUE : 0;
  _state_transition_time = nil_time;
  _velocity_low_time = nil_time;
  _velocity_high_time = nil_time;
  _just_pressed = false;
  _just_released = false;
  _last_velocity = std::nullopt;
  _last_reported_pressure = std::nullopt;
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

  if (is_pressure_tracked()) {
    report_pressure(value);
  }
}

void Drumpad::update_state_machine(std::uint16_t current_adc_value,
                                   absolute_time_t now) {
  uint64_t time_in_state = absolute_time_diff_us(_state_transition_time, now);

  switch (_current_state) {
  case DrumpadState::Idle:
    if (current_adc_value >= _noise_threshold) {
      _current_state = DrumpadState::Rising;
      _pressure_level = PressureLevel::None;
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
    // A dip below _trigger_threshold does not abort the hold: as long as
    // contact stays above _noise_threshold, the hold timer keeps running, so
    // a press that sags after impact still counts as held.
    if (time_in_state >= _hold_time_us) {
      _current_state = DrumpadState::Holding;
      notify_event(DrumpadEvent::Type::Hold, std::nullopt, current_adc_value);
    } else if (current_adc_value < _noise_threshold) {
      _current_state = DrumpadState::DebouncingRelease;
      _state_transition_time = now;
    }
    break;

  case DrumpadState::Falling:
    if (current_adc_value < _noise_threshold) {
      _current_state = DrumpadState::DebouncingRelease;
      _state_transition_time = now;
    } else if (current_adc_value >= _trigger_threshold) {
      resume_press(current_adc_value, now);
    }
    break;

  case DrumpadState::Holding:
    // Reaching Holding means the pad has been pressed and held past
    // _hold_time_us, so a pressure level is reported even if pressure has
    // already settled below _trigger_threshold. Above trigger the level
    // follows pressure in both directions: pressing harder gives Hard, easing
    // off returns to Light. Below trigger the level is left untouched: it is
    // preserved through Falling and only cleared on Release, so a held pad
    // keeps reporting its level for the whole press.
    if (current_adc_value >= _trigger_threshold) {
      _pressure_level = classify_pressure(current_adc_value);
    } else {
      if (_pressure_level == PressureLevel::None) {
        _pressure_level = PressureLevel::Light;
      }
      _current_state = DrumpadState::Falling;
    }
    break;

  case DrumpadState::DebouncingRelease:
    if (current_adc_value >= _noise_threshold) {
      _current_state = DrumpadState::Falling;
      _state_transition_time = now;
    } else if (time_in_state >= _debounce_time_us) {
      release(current_adc_value, now);
    }
    break;
  }
}

void Drumpad::release(std::uint16_t current_adc_value, absolute_time_t now) {
  report_pressure_released(current_adc_value);
  notify_event(DrumpadEvent::Type::Release, std::nullopt, current_adc_value);
  _current_state = DrumpadState::Idle;
  _pressure_level = PressureLevel::None;
  _state_transition_time = now;
  _just_released = true;
  _last_adc_value = 0;
  _velocity_low_time = nil_time;
  _velocity_high_time = nil_time;
}

// Pressure has climbed back above _trigger_threshold while Falling. If the
// hold already engaged, go straight back to Holding so the pressure level can
// follow pressure again. Otherwise resume the hold timer from the original
// Press, so a brief contact bounce cannot stall the pad short of Holding.
void Drumpad::resume_press(std::uint16_t current_adc_value,
                           absolute_time_t now) {
  if (_pressure_level != PressureLevel::None) {
    _current_state = DrumpadState::Holding;
    _pressure_level = classify_pressure(current_adc_value);
    return;
  }
  _state_transition_time = _velocity_high_time;
  if (absolute_time_diff_us(_velocity_high_time, now) >= _hold_time_us) {
    _current_state = DrumpadState::Holding;
    notify_event(DrumpadEvent::Type::Hold, std::nullopt, current_adc_value);
  } else {
    _current_state = DrumpadState::Peaking;
  }
}

PressureLevel
Drumpad::classify_pressure(std::uint16_t current_adc_value) const {
  return current_adc_value >= _high_pressure_threshold ? PressureLevel::Hard
                                                       : PressureLevel::Light;
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

// Continuous pressure is only reported once the hold has engaged, so it
// never races the Press that starts the note.
bool Drumpad::is_pressure_tracked() const {
  return _pressure_hysteresis > 0 && (_current_state == DrumpadState::Holding ||
                                      _current_state == DrumpadState::Falling);
}

void Drumpad::report_pressure(std::uint16_t current_adc_value) {
  uint8_t pressure = pressure_from_adc(current_adc_value);
  if (!_last_reported_pressure.has_value()) {
    emit_pressure(pressure, current_adc_value);
    return;
  }
  int delta = static_cast<int>(pressure) -
              static_cast<int>(_last_reported_pressure.value());
  if (delta >= _pressure_hysteresis || -delta >= _pressure_hysteresis) {
    emit_pressure(pressure, current_adc_value);
  }
}

// A note that was reported with pressure ends at zero pressure, so receivers
// are not left holding a stale value.
void Drumpad::report_pressure_released(std::uint16_t current_adc_value) {
  if (_last_reported_pressure.value_or(0) > 0) {
    emit_pressure(0, current_adc_value);
  }
  _last_reported_pressure = std::nullopt;
}

void Drumpad::emit_pressure(uint8_t pressure, std::uint16_t current_adc_value) {
  _last_reported_pressure = pressure;
  DrumpadEvent event{.pad_index = _pad_id,
                     .type = DrumpadEvent::Type::Pressure,
                     .velocity = std::nullopt,
                     .raw_value = current_adc_value,
                     .pressure = pressure};
  this->notify_observers(event);
}

uint8_t Drumpad::pressure_from_adc(std::uint16_t current_adc_value) const {
  if (current_adc_value <= _noise_threshold) {
    return 0;
  }
  uint32_t span = musin::hal::ADC_MAX_VALUE - _noise_threshold;
  uint32_t scaled = (current_adc_value - _noise_threshold) * 127u / span;
  return static_cast<uint8_t>(scaled > 127u ? 127u : scaled);
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
