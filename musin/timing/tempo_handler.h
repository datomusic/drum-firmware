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
 * @brief Manages tempo tracking and phase alignment.
 *
 * Observes SpeedAdapter for speed-modified clock events and emits TempoEvents
 * with phase information. Delegates clock source selection and speed control
 * to ClockRouter and SpeedAdapter.
 */
class TempoHandler
    : public etl::observer<musin::timing::ClockEvent>,
      public etl::observable<etl::observer<musin::timing::TempoEvent>,
                             MAX_TEMPO_OBSERVERS> {
public:
  explicit TempoHandler(ClockRouter &clock_router_ref,
                        SpeedAdapter &speed_adapter_ref,
                        bool send_midi_clock_when_stopped,
                        ClockSource initial_source = ClockSource::INTERNAL);

  // Prevent copying and assignment
  TempoHandler(const TempoHandler &) = delete;
  TempoHandler &operator=(const TempoHandler &) = delete;

  void set_clock_source(ClockSource source);
  [[nodiscard]] ClockSource get_clock_source() const;

  void set_bpm(float bpm);
  void set_speed_modifier(SpeedModifier modifier);
  void set_tempo_control_value(float knob_value);

  [[nodiscard]] SpeedModifier get_speed_modifier() const;

  void set_playback_state(PlaybackState new_state);
  [[nodiscard]] PlaybackState get_playback_state() const;

  void trigger_manual_sync(uint8_t target_phase = 0);

  void notification(musin::timing::ClockEvent event) override;

private:
  [[nodiscard]] uint8_t calculate_aligned_phase() const;
  void emit_manual_resync_event(uint8_t anchor_phase);

  ClockRouter &_clock_router_ref;
  SpeedAdapter &_speed_adapter_ref;

  PlaybackState _playback_state;
  SpeedModifier current_speed_modifier_;
  uint8_t phase_12_;
  uint64_t tick_count_;
  const bool _send_midi_clock_when_stopped;
  bool initialized_ = false;
  float last_tempo_knob_value_ = 0.5f;
  bool waiting_for_external_downbeat_ = false;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_TEMPO_HANDLER_H
