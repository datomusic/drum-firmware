#ifndef SB25_DRUM_DEBUG_UTILS_H
#define SB25_DRUM_DEBUG_UTILS_H

#include "pico/time.h"
#include <cstdint>

namespace DebugUtils {

class LoopTimer {
public:
    explicit LoopTimer(uint32_t print_interval_ms = 1000);

    void record_iteration_end();

private:
    absolute_time_t _last_print_time;
    absolute_time_t _last_loop_end_time;
    uint64_t _accumulated_loop_time_us;
    uint32_t _loop_count;
    uint64_t _print_interval_us;
};

} // namespace DebugUtils

#endif // SB25_DRUM_DEBUG_UTILS_H
