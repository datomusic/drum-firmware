#ifndef MUSIN_UI_DRUMPAD_H
#define MUSIN_UI_DRUMPAD_H

#include <cstdint>
#include <optional>

#include "drum/config.h"
#include "etl/observer.h"
#include "musin/hal/adc_defs.h" // For ADC_MAX_VALUE

extern "C" {
#include "pico/time.h"
}

namespace musin::ui {

struct DrumpadEvent {
  enum class Type : uint8_t {
    Press,
    Release,
    Hold
  };
  uint8_t pad_index;
  Type type;
  std::optional<uint8_t> velocity;
  uint16_t raw_value;
};

enum class DrumpadState : std::uint8_t {
  IDLE,
  RISING,
  PEAKING,
  FALLING,
  HOLDING,
  DEBOUNCING_RELEASE
};

enum class RetriggerMode : uint8_t {
  Off,
  Single,
  Double
};

class Drumpad : public etl::observable<etl::observer<DrumpadEvent>, 4> {
public:
  explicit Drumpad(uint8_t pad_id,
                   const drum::config::drumpad::DrumpadConfig &config);

  Drumpad(const Drumpad &) = delete;
  Drumpad &operator=(const Drumpad &) = delete;

  void init();
  void update(uint16_t raw_adc_value);

  bool was_pressed() const {
    return _just_pressed;
  }
  bool was_released() const {
    return _just_released;
  }
  bool is_held() const {
    return _current_state == DrumpadState::HOLDING;
  }
  std::optional<uint8_t> get_velocity() const {
    return _last_velocity;
  }
  std::uint16_t get_raw_adc_value() const {
    return _last_adc_value;
  }
  DrumpadState get_current_state() const {
    return _current_state;
  }
  RetriggerMode get_retrigger_mode() const {
    return _current_retrigger_mode;
  }
  uint8_t get_id() const {
    return _pad_id;
  }

private:
  void notify_event(DrumpadEvent::Type type, std::optional<uint8_t> velocity,
                    uint16_t raw_value);
  void update_state_machine(std::uint16_t current_adc_value,
                            absolute_time_t now);
  uint8_t calculate_velocity(uint64_t time_diff_us) const;

  const uint8_t _pad_id;
  const std::uint16_t _noise_threshold;
  const std::uint16_t _trigger_threshold;
  const std::uint16_t _double_retrigger_pressure_threshold;
  const bool _active_low;
  const std::uint32_t _debounce_time_us;
  const std::uint32_t _hold_time_us;
  const std::uint64_t _max_velocity_time_us;
  const std::uint64_t _min_velocity_time_us;

  DrumpadState _current_state = DrumpadState::IDLE;
  RetriggerMode _current_retrigger_mode = RetriggerMode::Off;
  std::uint16_t _last_adc_value = 0;
  absolute_time_t _state_transition_time = nil_time;
  absolute_time_t _velocity_low_time = nil_time;
  absolute_time_t _velocity_high_time = nil_time;

  bool _just_pressed = false;
  bool _just_released = false;
  std::optional<uint8_t> _last_velocity = std::nullopt;
};

} // namespace musin::ui

#endif // MUSIN_UI_DRUMPAD_H
