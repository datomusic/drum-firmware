#ifndef MUSIN_TIMING_SYNC_IN_H
#define MUSIN_TIMING_SYNC_IN_H

#include "etl/observer.h"
#include "musin/hal/gpio.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h" // For absolute_time_t and nil_time
#include <cstdint>

namespace musin::timing {

constexpr size_t MAX_SYNC_IN_OBSERVERS = 1;

class SyncIn : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                                      MAX_SYNC_IN_OBSERVERS> {
public:
  SyncIn(uint32_t sync_pin, uint32_t detect_pin);

  void update(absolute_time_t now);
  [[nodiscard]] bool is_cable_connected() const;

  void set_ppqn(uint32_t ppqn);
  [[nodiscard]] uint32_t get_ppqn() const;

private:
  uint32_t ppqn_ = 24;
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

  static constexpr uint32_t PULSE_DEBOUNCE_US = 5000;   // 5ms
  static constexpr uint32_t DETECT_DEBOUNCE_US = 50000; // 50ms
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SYNC_IN_H
