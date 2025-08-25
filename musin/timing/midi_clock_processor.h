#ifndef MUSIN_TIMING_MIDI_CLOCK_PROCESSOR_H
#define MUSIN_TIMING_MIDI_CLOCK_PROCESSOR_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h" // For absolute_time_t, etc.
#include <cstdint>

namespace musin::timing {

// Maximum number of observers MidiClockProcessor can notify (e.g.,
// TempoHandler)
constexpr size_t MAX_MIDI_CLOCK_PROCESSOR_OBSERVERS = 1;

/**
 * @brief Processes raw incoming MIDI clock ticks, forwards them, and detects
 * timeouts.
 */
class MidiClockProcessor
    : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_MIDI_CLOCK_PROCESSOR_OBSERVERS> {
public:
  MidiClockProcessor();

  // Prevent copying and assignment
  MidiClockProcessor(const MidiClockProcessor &) = delete;
  MidiClockProcessor &operator=(const MidiClockProcessor &) = delete;

  /**
   * @brief Called by the MIDI input layer upon receiving a raw MIDI clock tick
   * (0xF8).
   */
  void on_midi_clock_tick_received();

  /**
   * @brief Checks if the MIDI clock is currently active (receiving ticks within
   * the timeout).
   * @return true if the clock is active, false otherwise.
   */
  [[nodiscard]] bool is_active() const;

  /**
   * @brief Resets the processor's state.
   */
  void reset();

private:
  absolute_time_t _last_raw_tick_time;

  // Timeout for considering MIDI clock inactive.
  static constexpr uint32_t MIDI_CLOCK_TIMEOUT_US = 500000; // 500ms
};

} // namespace musin::timing

#endif // MUSIN_TIMING_MIDI_CLOCK_PROCESSOR_H
