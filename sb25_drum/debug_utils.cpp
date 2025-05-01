#include "debug_utils.h"
#include <stdio.h> // For printf

namespace DebugUtils {

LoopTimer::LoopTimer(uint32_t print_interval_ms)
    : _accumulated_loop_time_us(0), _loop_count(0),
      _print_interval_us(static_cast<uint64_t>(print_interval_ms) * 1000) {
  _last_print_time = get_absolute_time();
  _last_loop_end_time = _last_print_time;
}

void LoopTimer::record_iteration_end() {
  absolute_time_t current_time = get_absolute_time();

  uint64_t loop_duration_us = absolute_time_diff_us(_last_loop_end_time, current_time);
  _last_loop_end_time = current_time;

  _accumulated_loop_time_us += loop_duration_us;
  _loop_count++;

  // Cast the result of absolute_time_diff_us to uint64_t to match _print_interval_us type
  if (static_cast<uint64_t>(absolute_time_diff_us(_last_print_time, current_time)) >=
      _print_interval_us) {
    if (_loop_count > 0) {
      uint64_t average_loop_time_us = _accumulated_loop_time_us / _loop_count;
      printf("Avg loop time: %llu us (%lu loops)\n", average_loop_time_us, _loop_count);
    }

    _last_print_time = current_time;
    _accumulated_loop_time_us = 0;
    _loop_count = 0;
  }
}

} // namespace DebugUtils
