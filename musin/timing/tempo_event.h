#ifndef MUSIN_TIMING_TEMPO_EVENT_H
#define MUSIN_TIMING_TEMPO_EVENT_H

#include <cstdint>

namespace Musin::Timing {

/**
 * @brief Event structure carrying information about a tempo-related tick.
 * This is emitted by TempoHandler and potentially consumed by TempoMultiplier.
 */
struct TempoEvent {
  // uint64_t timestamp_us; // Example: Timestamp in microseconds
  // float bpm;             // Example: Current calculated BPM
  // uint32_t tick_count;   // Example: A running tick counter
  // For simplicity now, just an empty struct to signal a processed tick.
  // Add members as needed.
};

} // namespace Musin::Timing

#endif // MUSIN_TIMING_TEMPO_EVENT_H
