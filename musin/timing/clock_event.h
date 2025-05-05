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
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  musin::timing::ClockSource source;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_CLOCK_EVENT_H
