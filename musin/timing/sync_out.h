#ifndef MUSIN_TIMING_SYNC_OUT_H
#define MUSIN_TIMING_SYNC_OUT_H

#include "etl/observer.h"
#include "musin/hal/gpio.h"
#include "musin/timing/tempo_event.h"
#include "pico/time.h" // For alarm_id_t, absolute_time_t
#include <cstdint>

namespace musin::timing {

/**
 * @brief Generates synchronization pulses on a GPIO pin based on tempo events.
 *
 * The SyncOut driver observes TempoHandler and triggers a pulse of
 * configurable duration after a configurable number of tempo ticks.
 */
class SyncOut : public etl::observer<musin::timing::TempoEvent> {
public:
  /**
   * @brief Constructor for SyncOut.
   * @param gpio_pin The GPIO pin number to use for the sync output.
   * @param ticks_per_pulse The number of tempo ticks before a pulse is
   * generated. Default is 12 (generates 2 PPQN sync pulses).
   * @param pulse_duration_ms The duration of the sync pulse in milliseconds.
   * Default is 10ms.
   */
  SyncOut(std::uint32_t gpio_pin, std::uint32_t ticks_per_pulse = 12,
          std::uint32_t pulse_duration_ms = 10);

  ~SyncOut();

  // Prevent copying and moving
  SyncOut(const SyncOut &) = delete;
  SyncOut &operator=(const SyncOut &) = delete;
  SyncOut(SyncOut &&) = delete;
  SyncOut &operator=(SyncOut &&) = delete;

  /**
   * @brief Handles incoming tempo tick notifications.
   * @param event The tempo event (not used for data, only as a trigger).
   */
  void notification(musin::timing::TempoEvent event) override;

  /**
   * @brief Enables the sync pulse generation.
   * Must be attached to TempoHandler externally.
   */
  void enable();

  /**
   * @brief Disables the sync pulse generation.
   * Must be detached from TempoHandler externally.
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
  const std::uint32_t _ticks_per_pulse;
  const std::uint64_t _pulse_duration_us;
  bool _is_enabled;
  bool _pulse_active;
  alarm_id_t _pulse_alarm_id;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SYNC_OUT_H
