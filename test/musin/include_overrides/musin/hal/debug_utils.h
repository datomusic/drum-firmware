#ifndef MUSIN_HAL_DEBUG_UTILS_H_
#define MUSIN_HAL_DEBUG_UTILS_H_

#include <atomic>
#include <cstdint>

namespace musin::hal {
namespace DebugUtils {

// Global atomic counters for underrun monitoring.
// These are available in tests to be inspected if needed.
inline std::atomic<uint32_t> g_audio_output_underruns{0};
inline std::atomic<uint32_t> g_attack_buffer_reader_underruns{0};
inline std::atomic<uint32_t> g_pitch_shifter_underruns{0};

#if !defined(ENABLE_PROFILING)
#define ENABLE_PROFILING
#endif

// Stub for SectionProfiler class
template <size_t MaxSections>
class SectionProfiler {
public:
  explicit SectionProfiler(uint32_t /*print_interval_ms*/ = 2000) {}
  size_t add_section(const char* /*name*/) { return 0; }
  void record_duration(size_t /*index*/, uint64_t /*duration_us*/) {}
  void check_and_print_report() {}
};

// Stub for ScopedProfile class
template <size_t MaxSections>
class ScopedProfile {
public:
  ScopedProfile(SectionProfiler<MaxSections>& /*profiler*/, size_t /*section_index*/) {}
  ScopedProfile(const ScopedProfile&) = delete;
  ScopedProfile& operator=(const ScopedProfile&) = delete;
};

// Define the number of sections for the global profiler
constexpr size_t kGlobalProfilerMaxSections = 2;
// Stub for the global profiler instance
inline SectionProfiler<kGlobalProfilerMaxSections> g_section_profiler;

// Stub for LoopTimer
class LoopTimer {
public:
  explicit LoopTimer(uint32_t /*print_interval_ms*/ = 1000) {}
  void record_iteration_end() {}
};

} // namespace DebugUtils
} // namespace musin::hal

#endif // MUSIN_HAL_DEBUG_UTILS_H_
