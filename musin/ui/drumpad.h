#ifndef MUSIN_UI_DRUMPAD_H
#define MUSIN_UI_DRUMPAD_H

#include <cstdint>
#include <optional>

#include "etl/observer.h"
#include "musin/hal/adc_defs.h" // For ADC_MAX_VALUE
#include "musin/ui/drumpad_config.h"

extern "C" {
#include "pico/time.h"
}

namespace musin::ui {

struct DrumpadEvent {
  enum class Type : uint8_t {
    Press,
    Release,
    Hold,
    Pressure
  };
  uint8_t pad_index;
  Type type;
  std::optional<uint8_t> velocity;
  uint16_t raw_value;
  // 0-127, present on Pressure events only. Tracks the held pad's pressure
  // from noise_threshold (0) up to full-scale ADC (127).
  std::optional<uint8_t> pressure = std::nullopt;
};

enum class DrumpadState : std::uint8_t {
  Idle,
  Rising,
  Peaking,
  Falling,
  Holding,
  DebouncingRelease
};

/**
 * @brief How firmly a held pad is being pressed.
 *
 * None until the pad has been held past hold_time_us; then Light or Hard
 * depending on high_pressure_threshold, following pressure in both directions
 * while contact stays above trigger_threshold. Preserved while pressure sags
 * below trigger_threshold and cleared only on Release.
 */
enum class PressureLevel : uint8_t {
  None,
  Light,
  Hard
};

class Drumpad : public etl::observable<etl::observer<DrumpadEvent>, 4> {
public:
  explicit Drumpad(uint8_t pad_id, const DrumpadConfig &config);

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
    return _current_state == DrumpadState::Holding;
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
  PressureLevel get_pressure_level() const {
    return _pressure_level;
  }
  uint8_t get_id() const {
    return _pad_id;
  }

private:
  void notify_event(DrumpadEvent::Type type, std::optional<uint8_t> velocity,
                    uint16_t raw_value);
  void update_state_machine(std::uint16_t current_adc_value,
                            absolute_time_t now);
  void resume_press(std::uint16_t current_adc_value, absolute_time_t now);
  void release(std::uint16_t current_adc_value, absolute_time_t now);
  PressureLevel classify_pressure(std::uint16_t current_adc_value) const;
  uint8_t calculate_velocity(uint64_t time_diff_us) const;
  bool is_pressure_tracked() const;
  void report_pressure(std::uint16_t current_adc_value);
  void report_pressure_released(std::uint16_t current_adc_value);
  void emit_pressure(uint8_t pressure, std::uint16_t current_adc_value);
  uint8_t pressure_from_adc(std::uint16_t current_adc_value) const;

  const uint8_t _pad_id;
  const std::uint16_t _noise_threshold;
  const std::uint16_t _trigger_threshold;
  const std::uint16_t _high_pressure_threshold;
  const bool _active_low;
  const std::uint32_t _debounce_time_us;
  const std::uint32_t _hold_time_us;
  const std::uint64_t _max_velocity_time_us;
  const std::uint64_t _min_velocity_time_us;
  const uint8_t _pressure_hysteresis;

  DrumpadState _current_state = DrumpadState::Idle;
  PressureLevel _pressure_level = PressureLevel::None;
  std::uint16_t _last_adc_value = 0;
  absolute_time_t _state_transition_time = nil_time;
  absolute_time_t _velocity_low_time = nil_time;
  absolute_time_t _velocity_high_time = nil_time;

  bool _just_pressed = false;
  bool _just_released = false;
  std::optional<uint8_t> _last_velocity = std::nullopt;
  std::optional<uint8_t> _last_reported_pressure = std::nullopt;
};

} // namespace musin::ui

#endif // MUSIN_UI_DRUMPAD_H
