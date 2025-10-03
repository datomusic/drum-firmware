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
  static constexpr uint32_t PPQN = 24;

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
   * @brief Update method to be called from the main loop.
   * @param now The current time.
   */
  void update(absolute_time_t now);

  /**
   * @brief Reset the clock timing to align with current time.
   * This should be called when manually injecting ticks to avoid timing
   * conflicts.
   */
  void reset();

private:
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
  absolute_time_t _next_tick_time;
};

} // namespace musin::timing

#endif // MUSIN_HAL_INTERNAL_CLOCK_H
