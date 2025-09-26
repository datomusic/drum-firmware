#ifndef MUSIN_HAL_DEBUG_UTILS_H_
#define MUSIN_HAL_DEBUG_UTILS_H_

extern "C" {
#include "pico/malloc.h"
#include "pico/time.h"
}
#include "etl/array.h"
#include "etl/string.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <unistd.h> // For sbrk

#define ENABLE_PROFILING

// Linker script symbols for memory regions
extern "C" {
extern char __end__[];
extern char __HeapLimit[]; // End of the heap
extern char __StackLimit[];
extern char __StackTop[];
}

namespace musin::hal {
namespace DebugUtils {

// Global atomic counters for underrun monitoring
inline std::atomic<uint32_t> g_audio_output_underruns{0};
inline std::atomic<uint32_t> g_attack_buffer_reader_underruns{0};
inline std::atomic<uint32_t> g_pitch_shifter_underruns{0};

#ifdef ENABLE_PROFILING

// Helper to get current stack pointer
static inline void *get_current_sp() {
  void *sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  return sp;
}

/**
 * @brief Measures and reports the average execution time of registered code
 * sections.
 *
 * When ENABLE_PROFILING is defined, this class collects timing data for
 * different code sections identified by an index. It periodically prints a
 * report to the console showing the average execution time and call count for
 * each section. If ENABLE_PROFILING is not defined, this class compiles to
 * empty stubs, incurring no runtime overhead.
 *
 * @tparam MaxSections The maximum number of code sections that can be profiled.
 */
template <size_t MaxSections> class SectionProfiler {
  struct ProfiledSection {
    const char *name = nullptr;
    uint64_t accumulated_time_us = 0;
    uint32_t call_count = 0;
  };

public:
  explicit SectionProfiler(uint32_t print_interval_ms = 2000)
      : _print_interval_us(static_cast<uint64_t>(print_interval_ms) * 1000),
        _current_section_count(0), _last_print_time(get_absolute_time()) {
  }

  size_t add_section(const char *name) {
    if (_current_section_count < MaxSections) {
      _sections[_current_section_count].name = name;
      _sections[_current_section_count].accumulated_time_us = 0;
      _sections[_current_section_count].call_count = 0;
      return _current_section_count++;
    }
    printf("Error: Exceeded maximum number of profiled sections (%u)\n",
           MaxSections);
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
    if (static_cast<uint64_t>(absolute_time_diff_us(
            _last_print_time, current_time)) >= _print_interval_us) {
      print_report();
      _last_print_time = current_time;
    }
  }

private:
  void print_report() {
    printf("--- Profiling Report ---\n");
    for (size_t i = 0; i < _current_section_count; ++i) {
      if (_sections[i].call_count > 0) {
        uint64_t avg_time_us =
            _sections[i].accumulated_time_us / _sections[i].call_count;
        printf("Section '%s': Avg %llu us (%lu calls)\n",
               _sections[i].name ? _sections[i].name : "Unnamed", avg_time_us,
               _sections[i].call_count);
      } else {
        printf("Section '%s': (No calls)\n",
               _sections[i].name ? _sections[i].name : "Unnamed");
      }
      _sections[i].accumulated_time_us = 0;
      _sections[i].call_count = 0;
    }
    printf("------------------------\n");

    // Memory usage report
    printf("--- Memory Report ---\n");

    // Heap statistics
    char *heap_start_addr = __end__;
    char *heap_limit_addr = __HeapLimit; // Use the provided __HeapLimit
    size_t total_heap_size =
        static_cast<size_t>(heap_limit_addr - heap_start_addr);

    char *current_break = static_cast<char *>(sbrk(0));
    size_t used_heap_size = 0;
    if (current_break >= heap_start_addr && current_break <= heap_limit_addr) {
      used_heap_size = static_cast<size_t>(current_break - heap_start_addr);
    } else if (current_break == (char *)-1) {
      // sbrk error or heap not used/available via sbrk.
      // This might happen if the heap is not managed in a way sbrk can track,
      // or if sbrk is not implemented/supported fully for the target.
      // Report used_heap_size as 0 or an error indicator.
      // For now, we'll stick to 0 if current_break is out of expected range.
    }
    // Ensure used_heap_size does not exceed total_heap_size in case of sbrk
    // anomalies
    if (used_heap_size > total_heap_size) {
      used_heap_size = total_heap_size;
    }

    printf("Heap: Total %u B, Used %u B, Free %u B\n",
           static_cast<unsigned int>(total_heap_size),
           static_cast<unsigned int>(used_heap_size),
           static_cast<unsigned int>(total_heap_size - used_heap_size));

    // Stack statistics
    char *stack_limit = __StackLimit;
    char *stack_top = __StackTop; // Typically highest address, stack grows down
    size_t total_stack_size = static_cast<size_t>(stack_top - stack_limit);
    char *current_stack_pointer = static_cast<char *>(get_current_sp());
    size_t used_stack_size = 0;
    if (current_stack_pointer >= stack_limit &&
        current_stack_pointer <= stack_top) {
      used_stack_size = static_cast<size_t>(stack_top - current_stack_pointer);
    }

    printf("Stack: Total %u B, Used %u B, Free %u B\n",
           static_cast<unsigned int>(total_stack_size),
           static_cast<unsigned int>(used_stack_size),
           static_cast<unsigned int>(total_stack_size - used_stack_size));
    printf("------------------------\n");

    // Underrun report
    printf("--- Underrun Report ---\n");
    uint32_t audio_output_underruns = g_audio_output_underruns.exchange(0);
    uint32_t attack_buffer_underruns =
        g_attack_buffer_reader_underruns.exchange(0);
    uint32_t pitch_shifter_underruns = g_pitch_shifter_underruns.exchange(0);

    printf("AudioOutput Underruns: %u\n", audio_output_underruns);
    printf("AttackBufferReader Underruns: %u\n", attack_buffer_underruns);
    printf("PitchShifter Underruns: %u\n", pitch_shifter_underruns);
    printf("------------------------\n");
  }

  // Members ordered to match constructor initializer list
  uint64_t _print_interval_us;
  size_t _current_section_count;
  // Other members
  etl::array<ProfiledSection, MaxSections> _sections;
  absolute_time_t _last_print_time;
};

/**
 * @brief A RAII helper to automatically record the duration of a scope for
 * SectionProfiler.
 *
 * Create an instance of this class at the beginning of a scope you want to
 * profile. When the instance goes out of scope (e.g., at the end of a function
 * or block), its destructor records the elapsed time using the provided
 * SectionProfiler instance and section index. If ENABLE_PROFILING is not
 * defined, this class compiles to an empty stub.
 *
 * @tparam MaxSections The maximum number of sections supported by the
 * associated SectionProfiler.
 */
template <size_t MaxSections> class ScopedProfile {
public:
  ScopedProfile(SectionProfiler<MaxSections> &profiler, size_t section_index)
      : _profiler(profiler), _section_index(section_index),
        _start_time(get_absolute_time()) {
  }

  ~ScopedProfile() {
    absolute_time_t end_time = get_absolute_time();
    uint64_t duration_us = absolute_time_diff_us(_start_time, end_time);
    _profiler.record_duration(_section_index, duration_us);
  }

  ScopedProfile(const ScopedProfile &) = delete;
  ScopedProfile &operator=(const ScopedProfile &) = delete;

private:
  SectionProfiler<MaxSections> &_profiler;
  size_t _section_index;
  absolute_time_t _start_time;
};

// Define the number of sections for the global profiler
constexpr size_t kGlobalProfilerMaxSections =
    2; // Matches AudioEngine's previous usage
inline SectionProfiler<kGlobalProfilerMaxSections> g_section_profiler;

#else // ENABLE_PROFILING not defined

// Stub for SectionProfiler class
template <size_t MaxSections> class SectionProfiler {
public:
  explicit SectionProfiler(uint32_t /*print_interval_ms*/ = 2000) {
  }
  size_t add_section(const char * /*name*/) {
    return 0;
  }
  void record_duration(size_t /*index*/, uint64_t /*duration_us*/) {
  }
  void check_and_print_report() {
  }
};

// Stub for ScopedProfile class
template <size_t MaxSections> class ScopedProfile {
public:
  ScopedProfile(SectionProfiler<MaxSections> & /*profiler*/,
                size_t /*section_index*/) {
  }
  ScopedProfile(const ScopedProfile &) = delete;
  ScopedProfile &operator=(const ScopedProfile &) = delete;
};

// Define the number of sections for the global profiler (must match
// ENABLE_PROFILING case)
constexpr size_t kGlobalProfilerMaxSections = 2;
// Stub for the global profiler instance
inline SectionProfiler<kGlobalProfilerMaxSections> g_section_profiler;

#endif // ENABLE_PROFILING

/**
 * @brief Calculates and prints the average duration of a loop over a specified
 * interval.
 *
 * This utility helps monitor the performance of a main loop or other recurring
 * task by periodically printing the average time taken per iteration.
 */
#ifndef NDEBUG
class LoopTimer {
public:
  explicit LoopTimer(uint32_t print_interval_ms = 1000)
      : _accumulated_loop_time_us(0), _loop_count(0),
        _print_interval_us(static_cast<uint64_t>(print_interval_ms) * 1000),
        _last_print_time(get_absolute_time()),
        _last_loop_end_time(get_absolute_time()) {
  }

  void record_iteration_end() {
    absolute_time_t current_time = get_absolute_time();

    uint64_t loop_duration_us =
        absolute_time_diff_us(_last_loop_end_time, current_time);
    _last_loop_end_time = current_time;

    _accumulated_loop_time_us += loop_duration_us;
    _loop_count++;

    // Cast the result of absolute_time_diff_us to uint64_t to match
    // _print_interval_us type
    if (static_cast<uint64_t>(absolute_time_diff_us(
            _last_print_time, current_time)) >= _print_interval_us) {
      if (_loop_count > 0) {
        uint64_t average_loop_time_us = _accumulated_loop_time_us / _loop_count;
        printf("Avg loop time: %llu us (%u loops)\n", average_loop_time_us,
               _loop_count);
      }

      _last_print_time = current_time;
      _accumulated_loop_time_us = 0;
      _loop_count = 0;
    }
  }

private:
  absolute_time_t _last_print_time;
  absolute_time_t _last_loop_end_time;
  uint64_t _accumulated_loop_time_us;
  uint32_t _loop_count;
  uint64_t _print_interval_us;
};
#else  // NDEBUG is defined
class LoopTimer {
public:
  explicit LoopTimer(uint32_t /*print_interval_ms*/ = 1000) {
  }
  void record_iteration_end() {
  }
};
#endif // NDEBUG

} // namespace DebugUtils
} // namespace musin::hal

#endif // MUSIN_HAL_DEBUG_UTILS_H_
