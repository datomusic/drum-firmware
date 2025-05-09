#ifndef MUSIN_HAL_DEBUG_UTILS_H_
#define MUSIN_HAL_DEBUG_UTILS_H_

extern "C" {
#include "pico/time.h"
}
#include "etl/array.h"
#include "etl/string.h"
#include <cstdint>
#include <cstdio>


#define ENABLE_PROFILING

namespace musin::hal {
namespace DebugUtils {
#ifdef ENABLE_PROFILING

/**
 * @brief Measures and reports the average execution time of registered code sections.
 *
 * When ENABLE_PROFILING is defined, this class collects timing data for different
 * code sections identified by an index. It periodically prints a report to the
 * console showing the average execution time and call count for each section.
 * If ENABLE_PROFILING is not defined, this class compiles to empty stubs,
 * incurring no runtime overhead.
 *
 * @tparam MaxSections The maximum number of code sections that can be profiled.
 */
template <size_t MaxSections>
class SectionProfiler {
  struct ProfiledSection {
    const char* name = nullptr;
    uint64_t accumulated_time_us = 0;
    uint32_t call_count = 0;
  };

public:
  explicit SectionProfiler(uint32_t print_interval_ms = 2000)
      : _print_interval_us(static_cast<uint64_t>(print_interval_ms) * 1000),
        _current_section_count(0) {
    _last_print_time = get_absolute_time();
  }

  size_t add_section(const char* name) {
    if (_current_section_count < MaxSections) {
      _sections[_current_section_count].name = name;
      _sections[_current_section_count].accumulated_time_us = 0;
      _sections[_current_section_count].call_count = 0;
      return _current_section_count++;
    }
    printf("Error: Exceeded maximum number of profiled sections (%u)\n", MaxSections);
    return MaxSections;
  }

  void record_duration(size_t index, uint64_t duration_us) {
    if (index < _current_section_count) {
      _sections[index].accumulated_time_us += duration_us;
      _sections[index].call_count++;
    }
  }

  void check_and_print_report() {
    absolute_time_t current_time = get_absolute_time();
    if (static_cast<uint64_t>(absolute_time_diff_us(_last_print_time, current_time)) >=
        _print_interval_us) {
      print_report();
      _last_print_time = current_time;
    }
  }

private:
  void print_report() {
    printf("--- Profiling Report ---\n");
    for (size_t i = 0; i < _current_section_count; ++i) {
      if (_sections[i].call_count > 0) {
        uint64_t avg_time_us = _sections[i].accumulated_time_us / _sections[i].call_count;
        printf("Section '%s': Avg %llu us (%lu calls)\n",
               _sections[i].name ? _sections[i].name : "Unnamed", avg_time_us,
               _sections[i].call_count);
      } else {
        printf("Section '%s': (No calls)\n", _sections[i].name ? _sections[i].name : "Unnamed");
      }
      _sections[i].accumulated_time_us = 0;
      _sections[i].call_count = 0;
    }
    printf("------------------------\n");
  }

  etl::array<ProfiledSection, MaxSections> _sections;
  size_t _current_section_count;
  absolute_time_t _last_print_time;
  uint64_t _print_interval_us;
};

/**
 * @brief A RAII helper to automatically record the duration of a scope for SectionProfiler.
 *
 * Create an instance of this class at the beginning of a scope you want to profile.
 * When the instance goes out of scope (e.g., at the end of a function or block),
 * its destructor records the elapsed time using the provided SectionProfiler instance
 * and section index.
 * If ENABLE_PROFILING is not defined, this class compiles to an empty stub.
 *
 * @tparam MaxSections The maximum number of sections supported by the associated SectionProfiler.
 */
template <size_t MaxSections>
class ScopedProfile {
public:
  ScopedProfile(SectionProfiler<MaxSections>& profiler, size_t section_index)
      : _profiler(profiler), _section_index(section_index) {
    _start_time = get_absolute_time();
  }

  ~ScopedProfile() {
    absolute_time_t end_time = get_absolute_time();
    uint64_t duration_us = absolute_time_diff_us(_start_time, end_time);
    _profiler.record_duration(_section_index, duration_us);
  }

  ScopedProfile(const ScopedProfile&) = delete;
  ScopedProfile& operator=(const ScopedProfile&) = delete;

private:
  SectionProfiler<MaxSections>& _profiler;
  size_t _section_index;
  absolute_time_t _start_time;
};

#else

template <size_t MaxSections>
class SectionProfiler {
public:
  explicit SectionProfiler(uint32_t /*print_interval_ms*/ = 2000) {}
  size_t add_section(const char* /*name*/) { return 0; }
  void record_duration(size_t /*index*/, uint64_t /*duration_us*/) {}
  void check_and_print_report() {}
};

template <size_t MaxSections>
class ScopedProfile {
public:
  ScopedProfile(SectionProfiler<MaxSections>& /*profiler*/, size_t /*section_index*/) {}
  ScopedProfile(const ScopedProfile&) = delete;
  ScopedProfile& operator=(const ScopedProfile&) = delete;
};
#endif // ENABLE_PROFILING

/**
 * @brief Calculates and prints the average duration of a loop over a specified interval.
 *
 * This utility helps monitor the performance of a main loop or other recurring
 * task by periodically printing the average time taken per iteration.
 */
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
