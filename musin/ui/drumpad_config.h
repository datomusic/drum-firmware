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
};

} // namespace musin::ui

#endif // MUSIN_UI_DRUMPAD_CONFIG_H
