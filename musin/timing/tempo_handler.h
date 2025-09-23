#ifndef MUSIN_TIMING_TEMPO_HANDLER_H
#define MUSIN_TIMING_TEMPO_HANDLER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/speed_adapter.h"
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
constexpr size_t MAX_TEMPO_OBSERVERS = 4;

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
                        SyncIn &sync_in_ref, ClockRouter &clock_router_ref,
                        SpeedAdapter &speed_adapter_ref,
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
   * @brief Set tempo control value from knob position (0.0-1.0).
   * Automatically applies appropriate BPM or speed modifier based on current
   * clock source.
   * @param knob_value Normalized knob position (0.0-1.0).
   */
  void set_tempo_control_value(float knob_value);

  /**
   * @brief Get the current speed modifier.
   */
  [[nodiscard]] SpeedModifier get_speed_modifier() const {
    return current_speed_modifier_;
  }

  /**
   * @brief Set the current playback state.
   * @param new_state The new playback state.
   */
  void set_playback_state(PlaybackState new_state);

  /**
   * @brief Get the current playback state.
   * @return The current playback state (PLAYING or STOPPED).
   */
  [[nodiscard]] PlaybackState get_playback_state() const {
    return _playback_state;
  }

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
   * @brief Advance internal phase counter and emit tempo event.
   * This is the central method that advances phase_24_ and tick_count_,
   * then emits a TempoEvent with the current phase information.
   */
  void advance_phase_and_emit_event();

  InternalClock &_internal_clock_ref;
  MidiClockProcessor &_midi_clock_processor_ref;
  SyncIn &_sync_in_ref;
  ClockRouter &_clock_router_ref;
  SpeedAdapter &_speed_adapter_ref;

  ClockSource current_source_;
  PlaybackState _playback_state;
  SpeedModifier current_speed_modifier_;
  uint8_t phase_24_;    // 24 PPQN phase counter (0-23)
  uint64_t tick_count_; // Running tick count
  const bool _send_midi_clock_when_stopped;
  // For external sync alignment: alternate physical pulses between phases 0 and
  // 12
  bool external_align_to_12_next_ = false;

  // Deferred anchoring for MIDI: when true, next external tick will anchor
  // phase_24_ to 0/12 and emit a resync-marked event.
  bool pending_anchor_on_next_external_tick_ = false;
  bool pending_manual_resync_flag_ = false;

  // Timing for look-behind MIDI anchoring (lower 32 bits of us counter).
  // Unsigned subtraction handles rollover naturally.
  uint32_t last_external_tick_us_ = 0;
  uint32_t last_external_tick_interval_us_ = 0;

  // Tracks whether set_clock_source has performed initial attachment/setup.
  bool initialized_ = false;

  // Last tempo knob position for re-evaluation on clock source changes
  float last_tempo_knob_value_ = 0.5f;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_TEMPO_HANDLER_H
