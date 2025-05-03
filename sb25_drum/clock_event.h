#ifndef SB25_DRUM_CLOCK_EVENT_H
#define SB25_DRUM_CLOCK_EVENT_H

#include <cstdint>

// Forward declare the ClockSource enum from its new namespace
namespace Musin::Timing {
enum class ClockSource : uint8_t;
} // namespace Musin::Timing

namespace Musin::Timing {

/**
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  Musin::Timing::ClockSource source; // Identify which clock generated the tick
};

} // namespace Musin::Timing

#endif // SB25_DRUM_CLOCK_EVENT_H
