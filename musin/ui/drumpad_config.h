#ifndef MUSIN_UI_DRUMPAD_CONFIG_H
#define MUSIN_UI_DRUMPAD_CONFIG_H

#include <cstdint>

namespace musin::ui {

/**
 * @brief Per-pad tuning parameters for a pressure-sensitive drumpad.
 */
struct DrumpadConfig {
  uint16_t noise_threshold;
  uint16_t trigger_threshold;
  uint16_t high_pressure_threshold;
  bool active_low;
  uint32_t debounce_time_us;
  uint32_t hold_time_us;
  uint64_t max_velocity_time_us;
  uint64_t min_velocity_time_us;
  // Minimum change in 7-bit pressure before a new Pressure event is emitted
  // while the pad is held. 0 disables pressure reporting for this pad.
  uint8_t pressure_hysteresis;
};

} // namespace musin::ui

#endif // MUSIN_UI_DRUMPAD_CONFIG_H
