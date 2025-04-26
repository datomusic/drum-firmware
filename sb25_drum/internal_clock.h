#ifndef SB25_DRUM_INTERNAL_CLOCK_H
#define SB25_DRUM_INTERNAL_CLOCK_H

#include "clock_event.h"
#include "etl/observable.h"
#include "hardware/timer.h" // For alarm_pool_t, alarm_id_t, absolute_time_t
#include <cstdint>

namespace Clock {

// Maximum number of observers InternalClock can notify (e.g., TempoHandler)
constexpr size_t MAX_CLOCK_OBSERVERS = 2;

/**
 * @brief Generates clock ticks based on an internal timer and BPM setting.
 */
class InternalClock : public etl::observable<etl::observer<ClockEvent>, MAX_CLOCK_OBSERVERS> {
public:
  /**
   * @brief Pulses Per Quarter Note (PPQN). Standard MIDI clock is 24, common sequencer resolution is 96.
   */
  static constexpr uint32_t PPQN = 96;

  /**
   * @brief Constructor.
   * @param initial_bpm The starting Beats Per Minute.
   */
  explicit InternalClock(float initial_bpm = 120.0f);

  // Prevent copying and assignment
  InternalClock(const InternalClock &) = delete;
  InternalClock &operator=(const InternalClock &) = delete;

  /**
   * @brief Initialize the hardware timer. Must be called once.
   * @return True if initialization was successful, false otherwise.
   */
  bool init();

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
   * @param user_data Pointer to the InternalClock instance.
   * @return Reschedule interval in microseconds, or 0 to stop.
   */
  static int64_t timer_callback(alarm_id_t id, void *user_data);

  /**
   * @brief Instance-specific part of the timer callback logic.
   * @return Reschedule interval in microseconds, or 0 to stop.
   */
  int64_t handle_tick();

  /**
   * @brief Calculate the timer interval in microseconds based on current BPM and PPQN.
   */
  void calculate_interval();

  alarm_pool_t *_alarm_pool = nullptr;
  alarm_id_t _alarm_id = 0; // 0 indicates no active alarm
  float _current_bpm;
  int64_t _tick_interval_us = 0; // Interval between ticks in microseconds
  bool _is_running = false;
  bool _initialized = false;
};

} // namespace Clock

#endif // SB25_DRUM_INTERNAL_CLOCK_H
