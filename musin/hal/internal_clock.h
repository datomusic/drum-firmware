#ifndef SB25_DRUM_INTERNAL_CLOCK_H
#define SB25_DRUM_INTERNAL_CLOCK_H

#include "musin/timing/clock_event.h"
#include "etl/observer.h"
#include "musin/timing/timing_constants.h"
#include "pico/time.h" // Use pico_time for repeating_timer
#include <cstdint>

namespace Clock {

// Maximum number of observers InternalClock can notify (e.g., TempoHandler, PizzaControls)
constexpr size_t MAX_CLOCK_OBSERVERS = 3; // Increased from 2

/**
 * @brief Generates clock ticks based on an internal timer and BPM setting.
 */
class InternalClock : public etl::observable<etl::observer<Musin::Timing::ClockEvent>, MAX_CLOCK_OBSERVERS> {
public:
  /**
   * @brief Pulses Per Quarter Note (PPQN). Standard MIDI clock is 24, common sequencer resolution
   * is 96.
   */
  static constexpr uint32_t PPQN = Musin::Timing::DEFAULT_PPQN;

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
   * @brief Calculate the timer interval in microseconds based on current BPM and PPQN.
   */
  void calculate_interval();

  float _current_bpm;
  int64_t _tick_interval_us = 0; // Interval between ticks in microseconds
  bool _is_running = false;
  struct repeating_timer _timer_info; // Stores repeating timer state
};

} // namespace Clock

#endif // SB25_DRUM_INTERNAL_CLOCK_H
