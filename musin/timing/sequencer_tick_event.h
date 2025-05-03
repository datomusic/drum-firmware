#ifndef MUSIN_TIMING_TICK_EVENT_H
#define MUSIN_TIMING_TICK_EVENT_H

#include <cstdint>

namespace Musin::Timing {

/**
 * @brief Event structure signalling a tick for the sequencer to advance.
 * This is emitted by TempoMultiplier.
 */
struct SequencerTickEvent {
  // uint32_t step_index; // Example: The sequencer step index this tick corresponds to.
  // uint64_t timestamp_us; // Example: Timestamp of the tick event.
  // For simplicity now, just an empty struct to signal an advance tick.
};

} // namespace Musin::Timing

#endif // MUSIN_TIMING_TICK_EVENT_H
