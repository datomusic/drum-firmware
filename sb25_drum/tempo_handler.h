#ifndef SB25_DRUM_TEMPO_HANDLER_H
#define SB25_DRUM_TEMPO_HANDLER_H

#include "clock_event.h"
#include "etl/observer.h"
#include "tempo_event.h"
#include <cstdint>

// Forward declarations for Clock implementations
namespace Clock {
class InternalClock;
// class MIDIClock;
// class ExternalSyncClock;
}

namespace Tempo {

// Maximum number of observers TempoHandler can notify (e.g., TempoMultiplier)
constexpr size_t MAX_TEMPO_OBSERVERS = 2;

/**
 * @brief Defines the possible sources for the master clock signal.
 */
enum class ClockSource : uint8_t {
  INTERNAL,
  MIDI,
  EXTERNAL_SYNC
};

/**
 * @brief Manages the selection of the active clock source and forwards ticks.
 *
 * Listens to clock events from various sources (Internal, MIDI, External)
 * via the Observer pattern. Based on the selected `ClockSource`, it filters
 * and forwards valid tempo ticks to its own observers (e.g., TempoMultiplier)
 * as `TempoEvent`s.
 */
class TempoHandler : public etl::observer<Clock::ClockEvent>,
                     public etl::observable<etl::observer<TempoEvent>, MAX_TEMPO_OBSERVERS> {
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
  void notification(Clock::ClockEvent event) override;

private:
  ClockSource current_source_;

  // Pointers or references to actual clock instances might be needed here
  // if TempoHandler needs to interact with them directly (e.g., enable/disable).
  // Example:
  // Clock::InternalClock& internal_clock_;
  // Clock::MIDIClock& midi_clock_;
  // Clock::ExternalSyncClock& external_sync_clock_;
  // These would likely be passed in the constructor.
};

} // namespace Tempo

#endif // SB25_DRUM_TEMPO_HANDLER_H
