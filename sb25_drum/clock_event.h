#ifndef SB25_DRUM_CLOCK_EVENT_H
#define SB25_DRUM_CLOCK_EVENT_H

#include "tempo_handler.h" // Include definition for Tempo::ClockSource
#include <cstdint>

namespace Clock {

/**
 * @brief Event structure carrying information about a clock tick.
 */
struct ClockEvent {
  Tempo::ClockSource source; // Identify which clock generated the tick
};

} // namespace Clock

#endif // SB25_DRUM_CLOCK_EVENT_H
