#ifndef MUSIN_TIMING_MIDI_CLOCK_PROCESSOR_H
#define MUSIN_TIMING_MIDI_CLOCK_PROCESSOR_H

#include "etl/array.h"
#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/timing_constants.h" // For DEFAULT_PPQN (though MIDI clock is fixed at 24)
#include "pico/time.h"                     // For absolute_time_t, etc.
#include <cstdint>
#include <numeric> // For std::accumulate

namespace musin::timing {

// Maximum number of observers MidiClockProcessor can notify (e.g., TempoHandler)
constexpr size_t MAX_MIDI_CLOCK_PROCESSOR_OBSERVERS = 1;
// Number of raw tick intervals to average for BPM. MIDI clock is 24 PPQN.
// Averaging over one quarter note (24 ticks) can provide a good balance.
constexpr size_t MIDI_CLOCK_INTERVAL_HISTORY_SIZE = 24;

/**
 * @brief Processes raw incoming MIDI clock ticks to derive a stable tempo and generate ClockEvents.
 */
class MidiClockProcessor : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                                                  MAX_MIDI_CLOCK_PROCESSOR_OBSERVERS> {
public:
  MidiClockProcessor();

  // Prevent copying and assignment
  MidiClockProcessor(const MidiClockProcessor &) = delete;
  MidiClockProcessor &operator=(const MidiClockProcessor &) = delete;

  /**
   * @brief Called by the MIDI input layer upon receiving a raw MIDI clock tick (0xF8).
   */
  void on_midi_clock_tick_received();

  /**
   * @brief Gets the BPM derived from the incoming MIDI clock.
   * @return Derived BPM, or 0.0f if not enough data or clock is inactive/timed out.
   */
  [[nodiscard]] float get_derived_bpm() const;

  /**
   * @brief Resets the processor's state (e.g., when MIDI clock stops or source changes).
   * This should be called by TempoHandler when switching away from MIDI clock source.
   */
  void reset();

private:
  void update_derived_bpm();
  void schedule_next_stable_tick(absolute_time_t current_raw_tick_time);

  etl::array<uint32_t, MIDI_CLOCK_INTERVAL_HISTORY_SIZE> _raw_tick_intervals_us;
  size_t _interval_history_index;
  size_t _interval_history_count;
  absolute_time_t _last_raw_tick_time;

  float _derived_bpm;
  uint32_t _average_interval_us; // Smoothed interval based on history

  // For generating stable output ticks based on the derived average interval
  absolute_time_t _next_stable_tick_target_time;

  // Timeout for considering MIDI clock inactive. MIDI spec suggests 300ms for Active Sensing.
  // A slightly longer timeout might be practical for clock.
  static constexpr uint32_t MIDI_CLOCK_TIMEOUT_US = 500000; // 500ms
};

} // namespace musin::timing

#endif // MUSIN_TIMING_MIDI_CLOCK_PROCESSOR_H
