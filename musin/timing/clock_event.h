#ifndef MUSIN_TIMING_CLOCK_EVENT_H
#define MUSIN_TIMING_CLOCK_EVENT_H

#include <cstdint>

namespace musin::timing {

/**
 * @brief Defines the possible sources for the master clock signal.
 */
enum class ClockSource : uint8_t {
  INTERNAL,
  MIDI,
  EXTERNAL_SYNC
};

/**
 * @brief Defines speed modifiers for external clock sources.
 */
enum class SpeedModifier : uint8_t {
  HALF_SPEED = 1,
  NORMAL_SPEED = 2,
  DOUBLE_SPEED = 3
};

/**
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  musin::timing::ClockSource source;
  bool is_resync = false;  // True when clock resumes after timeout
};

} // namespace musin::timing

#endif // MUSIN_TIMING_CLOCK_EVENT_H
