#ifndef SB25_DRUM_CLOCK_EVENT_H
#define SB25_DRUM_CLOCK_EVENT_H

#include <cstdint>

// Forward declare the ClockSource enum instead of including the full header
namespace Tempo {
enum class ClockSource : uint8_t;
} // namespace Tempo

namespace Clock {

/**
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  Tempo::ClockSource source; // Identify which clock generated the tick
};

} // namespace Clock

#endif // SB25_DRUM_CLOCK_EVENT_H
