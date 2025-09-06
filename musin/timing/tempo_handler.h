#ifndef MUSIN_TIMING_TEMPO_HANDLER_H
#define MUSIN_TIMING_TEMPO_HANDLER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/tempo_event.h"
#include <cstdint>

namespace musin::timing {

// Forward declarations
class MidiClockProcessor;
class SyncIn;
class ClockMultiplier;

/**
 * @brief Defines the playback state of the sequencer.
 */
enum class PlaybackState : uint8_t {
  STOPPED,
  PLAYING
};

// Maximum number of observers TempoHandler can notify (e.g.,
// SequencerController)
constexpr size_t MAX_TEMPO_OBSERVERS = 3;

/**
 * @brief Manages the selection of the active clock source and forwards ticks.
 *
 * Listens to clock events from various sources (Internal, MIDI, External)
 * via the Observer pattern. Based on the selected `ClockSource`, it filters
 * and forwards valid tempo ticks to its own observers as `TempoEvent`s.
 */
class TempoHandler
    : public etl::observer<musin::timing::ClockEvent>,
      public etl::observable<etl::observer<musin::timing::TempoEvent>,
                             MAX_TEMPO_OBSERVERS> {
public:
  /**
   * @brief Constructor.
   * @param internal_clock_ref Reference to the InternalClock instance.
   * @param midi_clock_processor_ref Reference to the MidiClockProcessor.
   * @param sync_in_ref Reference to the SyncIn instance.
   * @param send_midi_clock_when_stopped If true, MIDI clock is sent even when
   * stopped.
   * @param initial_source The clock source to use initially.
   */
  explicit TempoHandler(InternalClock &internal_clock_ref,
                        MidiClockProcessor &midi_clock_processor_ref,
                        SyncIn &sync_in_ref,
                        ClockMultiplier &clock_multiplier_ref,
                        bool send_midi_clock_when_stopped,
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
   * @param event The received clock event.
   */
  void notification(musin::timing::ClockEvent event);

  /**
   * @brief Set the tempo for the internal clock.
   * @param bpm Beats Per Minute.
   */
  void set_bpm(float bpm);

  /**
   * @brief Set the speed modifier for all external clock sources (MIDI and
   * Sync).
   * @param modifier The speed modifier to apply (half, normal, double speed).
   */
  void set_speed_modifier(SpeedModifier modifier);

  /**
   * @brief Set the current playback state.
   * @param new_state The new playback state.
   */
  void set_playback_state(PlaybackState new_state);

  /**
   * @brief Periodically called to update internal state, like auto-switching
   * clock source.
   */
  void update();

  /**
   * @brief Trigger manual sync behavior when using external clock sources.
   * Sends resync event to observers for immediate phase alignment.
   */
  void trigger_manual_sync();

private:
  /**
   * @brief Process external clock tick with speed modifier applied
   * consistently. Consolidates MIDI and SYNC speed modifier logic for better
   * phase alignment.
   */
  void process_external_tick_with_speed_modifier();

  InternalClock &_internal_clock_ref;
  MidiClockProcessor &_midi_clock_processor_ref;
  SyncIn &_sync_in_ref;
  ClockMultiplier &_clock_multiplier_ref;

  ClockSource current_source_;
  PlaybackState _playback_state;
  SpeedModifier current_speed_modifier_;
  uint8_t tick_counter_;
  bool _send_this_internal_tick_as_midi_clock;
  const bool _send_midi_clock_when_stopped;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_TEMPO_HANDLER_H