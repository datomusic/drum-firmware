#ifndef MUSIN_TIMING_CLOCK_EVENT_H
#define MUSIN_TIMING_CLOCK_EVENT_H

#include <cstdint>

namespace Musin::Timing {
enum class ClockSource : uint8_t;
} // namespace Musin::Timing

namespace Musin::Timing {

/**
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  Musin::Timing::ClockSource source;
};

} // namespace Musin::Timing

#endif // MUSIN_TIMING_CLOCK_EVENT_H
