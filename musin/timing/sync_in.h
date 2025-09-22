#ifndef MUSIN_TIMING_SYNC_IN_H
#define MUSIN_TIMING_SYNC_IN_H

#include "etl/observer.h"
#include "musin/hal/gpio.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/timing_constants.h"
#include "pico/time.h" // For absolute_time_t and nil_time
#include <cstdint>

namespace musin::timing {

constexpr size_t MAX_SYNC_IN_OBSERVERS = 1;

/**
 * Handles external sync input, providing three main functions:
 * 1. Debounces the incoming physical sync pulse (2 PPQN).
 * 2. Detects whether the sync cable is connected (active-low).
 * 3. Converts the 2 PPQN signal to a 24 PPQN clock signal by emitting 11
 *    interpolated ticks between each physical pulse.
 *
 * Note on initial sync: A timing interval cannot be established until two
 * physical pulses have been received. Therefore, full 24 PPQN clock output
 * begins after the second physical pulse.
 */
class SyncIn : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                                      MAX_SYNC_IN_OBSERVERS> {
public:
  SyncIn(uint32_t sync_pin, uint32_t detect_pin);

  void update(absolute_time_t now);
  [[nodiscard]] bool is_cable_connected() const;

private:
  enum class PulseDebounceState {
    WAITING_FOR_RISING_EDGE,
    WAITING_FOR_STABLE_LOW
  };

  musin::hal::GpioPin sync_pin_;
  musin::hal::GpioPin detect_pin_;

  // For pulse debouncing
  PulseDebounceState pulse_state_ = PulseDebounceState::WAITING_FOR_RISING_EDGE;
  absolute_time_t falling_edge_time_ = nil_time;

  // For cable detection debouncing
  mutable bool last_detect_state_ = false;
  mutable absolute_time_t last_detect_change_time_ = nil_time;
  mutable bool current_detect_state_ = false;

  // For 24 PPQN conversion from 2 PPQN physical pulses
  absolute_time_t last_physical_pulse_time_ = nil_time;
  uint64_t tick_interval_us_ = 0;         // Interval for 24 PPQN ticks
  uint8_t interpolated_tick_counter_ = 0; // 0-11, tracks interpolated ticks
  absolute_time_t next_tick_time_ = nil_time;

  static constexpr uint32_t PULSE_DEBOUNCE_US = 5000;   // 5ms
  static constexpr uint32_t DETECT_DEBOUNCE_US = 50000; // 50ms
  static constexpr uint8_t PPQN_MULTIPLIER = 12;        // 2 PPQN to 24 PPQN

  void emit_clock_event(absolute_time_t timestamp, bool is_physical);
  void schedule_interpolated_ticks(absolute_time_t now);
  void emit_scheduled_ticks(absolute_time_t now);
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SYNC_IN_H
