#ifndef MUSIN_TIMING_SYNC_OUT_H
#define MUSIN_TIMING_SYNC_OUT_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include <cstdint>

namespace musin::timing {

// Lightweight test stub that avoids hardware GPIO and alarms.
class SyncOut : public etl::observer<musin::timing::ClockEvent> {
public:
  SyncOut(std::uint32_t /*gpio_pin*/, std::uint32_t /*ticks_per_pulse*/ = 12,
          std::uint32_t /*pulse_duration_ms*/ = 10) {
  }

  void notification([[maybe_unused]] musin::timing::ClockEvent event) override {
  }

  void enable() {
  }

  void disable() {
  }

  [[nodiscard]] bool is_enabled() const {
    return false;
  }

  void resync() {
  }
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SYNC_OUT_H
