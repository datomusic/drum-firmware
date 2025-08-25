#ifndef MUSIN_TIMING_SIMPLE_MIDI_CLOCK_PROCESSOR_H
#define MUSIN_TIMING_SIMPLE_MIDI_CLOCK_PROCESSOR_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h"
#include <cstdint>

namespace musin::timing {

constexpr size_t MAX_SIMPLE_MIDI_CLOCK_OBSERVERS = 1;

/**
 * @brief A simple processor that receives raw MIDI clock ticks and forwards
 * them as ClockEvents.
 *
 * This class does not perform any tempo derivation or tick stabilization. Its
 * sole responsibility is to notify observers when a MIDI clock message (0xF8)
 * is received and to track activity for source switching.
 */
class SimpleMidiClockProcessor
    : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_SIMPLE_MIDI_CLOCK_OBSERVERS> {
public:
  SimpleMidiClockProcessor();

  SimpleMidiClockProcessor(const SimpleMidiClockProcessor &) = delete;
  SimpleMidiClockProcessor &
  operator=(const SimpleMidiClockProcessor &) = delete;

  /**
   * @brief Called by the MIDI input layer upon receiving a raw MIDI clock tick.
   */
  void on_midi_clock_tick_received();

  /**
   * @brief Checks if a MIDI clock tick has been received recently.
   * @return True if the clock is considered active, false otherwise.
   */
  [[nodiscard]] bool is_active() const;

private:
  absolute_time_t last_tick_time_;

  // Timeout for considering the MIDI clock inactive.
  static constexpr uint32_t MIDI_CLOCK_TIMEOUT_US = 500000; // 500ms
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SIMPLE_MIDI_CLOCK_PROCESSOR_H
