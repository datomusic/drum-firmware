#ifndef MUSIN_TIMING_TEMPO_EVENT_H
#define MUSIN_TIMING_TEMPO_EVENT_H

#include <cstdint>

namespace musin::timing {

/**
 * @brief Event structure carrying information about a tempo-related tick.
 * This is emitted by TempoHandler and potentially consumed by TempoMultiplier.
 */
struct TempoEvent {
  uint64_t tick_count;
  uint8_t phase_24 = 0;
  bool is_resync = false;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_TEMPO_EVENT_H
