#ifndef MUSIN_TIMING_TEMPO_HANDLER_H
#define MUSIN_TIMING_TEMPO_HANDLER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_event.h"
#include <cstdint>

#include "musin/timing/clock_event.h" // Include for ClockSource
#include "musin/timing/internal_clock.h"

namespace musin::timing {

// Forward declaration for MidiClockProcessor
class MidiClockProcessor;

/**
 * @brief Defines the playback state of the sequencer.
 */
enum class PlaybackState : uint8_t {
  STOPPED,
  PLAYING
};

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
    : public etl::observer<musin::timing::ClockEvent>,
      public etl::observable<etl::observer<musin::timing::TempoEvent>, MAX_TEMPO_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param internal_clock_ref Reference to the InternalClock instance.
   * @param midi_clock_processor_ref Reference to the MidiClockProcessor instance.
   * @param initial_source The clock source to use initially.
   */
  explicit TempoHandler(InternalClock &internal_clock_ref,
                        MidiClockProcessor &midi_clock_processor_ref,
                        ClockSource initial_source = ClockSource::INTERNAL);

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
  void notification(musin::timing::ClockEvent event);

  /**
   * @brief Set the tempo for the internal clock, if it's the active source.
   * @param bpm Beats Per Minute.
   */
  void set_bpm(float bpm);

  /**
   * @brief Set the current playback state.
   * @param new_state The new playback state.
   */
  void set_playback_state(PlaybackState new_state);

  /**
   * @brief Periodically called to update internal state, like auto-switching clock source.
   * Should be called from the main application loop.
   */
  void update();

private:
  InternalClock &_internal_clock_ref;
  MidiClockProcessor &_midi_clock_processor_ref;
  ClockSource current_source_;
  PlaybackState _playback_state;
  bool _send_this_internal_tick_as_midi_clock;

  // Pointers or references to actual clock instances might be needed here
  // if TempoHandler needs to interact with them directly (e.g., enable/disable).
  // Example:
  // Clock::InternalClock& internal_clock_;
  // Clock::MIDIClock& midi_clock_;
  // Clock::ExternalSyncClock& external_sync_clock_;
  // These would likely be passed in the constructor.

}; // End class TempoHandler

} // namespace musin::timing

#endif // MUSIN_TIMING_TEMPO_HANDLER_H
