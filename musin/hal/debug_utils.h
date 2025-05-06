#ifndef MUSIN_HAL_DEBUG_UTILS_H_
#define MUSIN_HAL_DEBUG_UTILS_H_

#include "pico/time.h"
#include <cstdint>

namespace musin::hal {
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
} // namespace musin::hal

#endif // MUSIN_HAL_DEBUG_UTILS_H_
