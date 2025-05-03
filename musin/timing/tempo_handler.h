#ifndef MUSIN_TIMING_TEMPO_HANDLER_H
#define MUSIN_TIMING_TEMPO_HANDLER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_event.h"
#include <cstdint>

#include "musin/timing/clock_event.h" // Include for ClockSource

namespace Musin::Timing {

// Maximum number of observers TempoHandler can notify (e.g., TempoMultiplier, PizzaControls)
constexpr size_t MAX_TEMPO_OBSERVERS = 3;

/**
 * @brief Manages the selection of the active clock source and forwards ticks.
 *
 * Listens to clock events from various sources (Internal, MIDI, External)
 * via the Observer pattern. Based on the selected `ClockSource`, it filters
 * and forwards valid tempo ticks to its own observers (e.g., TempoMultiplier)
 * as `TempoEvent`s.
 */
class TempoHandler
    : public etl::observer<Musin::Timing::ClockEvent>,
      public etl::observable<etl::observer<Musin::Timing::TempoEvent>, MAX_TEMPO_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param initial_source The clock source to use initially.
   */
  explicit TempoHandler(ClockSource initial_source = ClockSource::INTERNAL);

  // Prevent copying and assignment
  TempoHandler(const TempoHandler &) = delete;
  TempoHandler &operator=(const TempoHandler &) = delete;

  /**
   * @brief Set the active clock source.
   * @param source The new clock source to use.
   */
  void set_clock_source(ClockSource source);

  /**
   * @brief Get the currently active clock source.
   * @return The current ClockSource enum value.
   */
  [[nodiscard]] ClockSource get_clock_source() const;

  /**
   * @brief Notification handler called when a ClockEvent is received.
   * Implements the etl::observer interface.
   * @param event The received clock event.
   */
  void notification(Musin::Timing::ClockEvent event);

private:
  ClockSource current_source_;

  // Pointers or references to actual clock instances might be needed here
  // if TempoHandler needs to interact with them directly (e.g., enable/disable).
  // Example:
  // Clock::InternalClock& internal_clock_;
  // Clock::MIDIClock& midi_clock_;
  // Clock::ExternalSyncClock& external_sync_clock_;
  // These would likely be passed in the constructor.

}; // End class TempoHandler

} // namespace Musin::Timing

#endif // MUSIN_TIMING_TEMPO_HANDLER_H
