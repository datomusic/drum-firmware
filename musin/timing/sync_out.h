#ifndef MUSIN_TIMING_SYNC_OUT_H
#define MUSIN_TIMING_SYNC_OUT_H

#include "musin/hal/gpio.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/internal_clock.h" // For InternalClock reference
#include "etl/observer.h"
#include "pico/time.h" // For alarm_id_t, absolute_time_t
#include <cstdint>

namespace musin::timing {

/**
 * @brief Generates synchronization pulses on a GPIO pin based on an InternalClock.
 *
 * The SyncOut driver observes an InternalClock and triggers a pulse of configurable
 * duration after a configurable number of clock ticks.
 */
class SyncOut : public etl::observer<musin::timing::ClockEvent> {
public:
  /**
   * @brief Constructor for SyncOut.
   * @param gpio_pin The GPIO pin number to use for the sync output.
   * @param clock_source A reference to the InternalClock instance to observe.
   * @param ticks_per_pulse The number of internal clock ticks before a pulse is generated. Default is 48.
   * @param pulse_duration_ms The duration of the sync pulse in milliseconds. Default is 10ms.
   */
  SyncOut(std::uint32_t gpio_pin, musin::timing::InternalClock &clock_source,
          std::uint32_t ticks_per_pulse = 12, std::uint32_t pulse_duration_ms = 10);

  ~SyncOut();

  // Prevent copying and moving
  SyncOut(const SyncOut &) = delete;
  SyncOut &operator=(const SyncOut &) = delete;
  SyncOut(SyncOut &&) = delete;
  SyncOut &operator=(SyncOut &&) = delete;

  /**
   * @brief Handles incoming clock tick notifications.
   * @param event The clock event (not used for data, only as a trigger).
   */
  void notification(musin::timing::ClockEvent event) override;

  /**
   * @brief Enables the sync pulse generation.
   * Attaches to the InternalClock as an observer.
   */
  void enable();

  /**
   * @brief Disables the sync pulse generation.
   * Detaches from the InternalClock and ensures any active pulse is terminated.
   */
  void disable();

  /**
   * @brief Checks if the sync pulse generation is currently enabled.
   * @return True if enabled, false otherwise.
   */
  [[nodiscard]] bool is_enabled() const;

private:
  static int64_t pulse_off_alarm_callback(alarm_id_t id, void *user_data);
  void trigger_pulse_off();

  musin::hal::GpioPin _gpio;
  musin::timing::InternalClock &_clock_source;
  const std::uint32_t _ticks_per_pulse;
  const std::uint64_t _pulse_duration_us; // Store in microseconds for precision with alarms
  std::uint32_t _tick_counter;
  bool _is_enabled;
  bool _pulse_active; // True if the GPIO pin is currently high
  alarm_id_t _pulse_alarm_id; // Stores the ID of the alarm used to turn the pulse off
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SYNC_OUT_H
