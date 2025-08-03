#ifndef MUSIN_HAL_INTERNAL_CLOCK_H
#define MUSIN_HAL_INTERNAL_CLOCK_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/timing_constants.h"
#include <atomic>
#include <cstdint>

extern "C" {
#include "pico/time.h"
}

namespace musin::timing {

// Maximum number of observers InternalClock can notify (e.g., TempoHandler,
// PizzaControls)
constexpr size_t MAX_CLOCK_OBSERVERS = 3;

/**
 * @brief Generates clock ticks based on an internal timer and BPM setting.
 */
class InternalClock
    : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_CLOCK_OBSERVERS> {
public:
  /**
   * @brief Pulses Per Quarter Note (PPQN). Standard MIDI clock is 24, common
   * sequencer resolution is 96.
   */
  static constexpr uint32_t PPQN = musin::timing::DEFAULT_PPQN;

  /**
   * @brief Constructor.
   * @param initial_bpm The starting Beats Per Minute.
   */
  explicit InternalClock(float initial_bpm = 120.0f);

  // Prevent copying and assignment
  InternalClock(const InternalClock &) = delete;
  InternalClock &operator=(const InternalClock &) = delete;

  /**
   * @brief Set the tempo.
   * @param bpm Beats Per Minute.
   */
  void set_bpm(float bpm);

  /**
   * @brief Get the current tempo.
   * @return Current Beats Per Minute.
   */
  [[nodiscard]] float get_bpm() const;

  /**
   * @brief Start generating clock ticks.
   * Requires init() to have been called successfully.
   */
  void start();

  /**
   * @brief Stop generating clock ticks.
   */
  void stop();

  /**
   * @brief Check if the clock is currently running.
   */
  [[nodiscard]] bool is_running() const;

  /**
   * @brief Sets the external source that this clock should discipline itself to.
   *
   * When a discipline source is set, the internal clock will use a PLL to
   * adjust its phase and frequency to match the incoming ticks from that
   * source.
   *
   * @param source The external clock source to follow (MIDI, EXTERNAL_SYNC).
   *               Set to INTERNAL to disable disciplining and run freely.
   * @param ppqn The Pulses Per Quarter Note of the external source. This is
   *             needed to correctly calculate phase errors.
   */
  void set_discipline(ClockSource source, uint32_t ppqn);

  /**
   * @brief Informs the clock that a reference tick has been received from an
   * external source.
   *
   * This is the primary input to the PLL. The clock will compare the arrival
   * time of this tick with its own internal phase to calculate the error and
   * adjust its frequency.
   *
   * @param now The timestamp when the reference tick was received.
   * @param source The source of the tick (MIDI or EXTERNAL_SYNC).
   */
  void reference_tick_received(absolute_time_t now, ClockSource source);

private:
  /**
   * @brief Static timer callback function required by the Pico SDK.
   * @param id Alarm ID.
   * @param rt Pointer to the repeating_timer structure.
   * @return True to continue repeating, false to stop.
   */
  static bool timer_callback(struct repeating_timer *rt);

  // handle_tick() logic moved into timer_callback

  /**
   * @brief Calculate the timer interval in microseconds for a given BPM and
   * current PPQN.
   * @param bpm The beats per minute to calculate interval for.
   * @return The calculated interval in microseconds, or 0 if BPM is invalid.
   */
  int64_t calculate_tick_interval(float bpm) const;

  float _current_bpm;
  int64_t _tick_interval_us = 0;
  bool _is_running = false;
  struct repeating_timer _timer_info; // Stores repeating timer state

  // For pending BPM changes
  volatile float _pending_bpm = 0.0f; // Written by main thread, read by ISR
  volatile int64_t _pending_tick_interval_us =
      0; // Written by main thread, read by ISR
  std::atomic<bool> _bpm_change_pending{
      false}; // Synchronizes access to pending values
};

} // namespace musin::timing

#endif // MUSIN_HAL_INTERNAL_CLOCK_H
