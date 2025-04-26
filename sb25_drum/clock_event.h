#ifndef SB25_DRUM_CLOCK_EVENT_H
#define SB25_DRUM_CLOCK_EVENT_H

#include <cstdint>

namespace Clock {

// Forward declare ClockSource enum if it lives elsewhere,
// or define it here/include its header if appropriate.
// For now, using uint8_t as a placeholder for source identification.
// enum class ClockSource : uint8_t;

/**
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  // uint64_t timestamp_us; // Example: Timestamp in microseconds
  // ClockSource source;    // Example: Identifier for the clock source
  // For simplicity now, just an empty struct to signal a tick.
  // Add members as needed (e.g., timestamp, source ID).
};

} // namespace Clock

#endif // SB25_DRUM_CLOCK_EVENT_H
