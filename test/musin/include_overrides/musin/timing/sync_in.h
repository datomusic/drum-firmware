#ifndef MUSIN_TIMING_SYNC_IN_H
#define MUSIN_TIMING_SYNC_IN_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h" // For absolute_time_t and nil_time
#include <cstdint>

namespace musin::timing {

// Match observable signature so TempoHandler can attach if needed
constexpr size_t MAX_SYNC_IN_OBSERVERS = 1;

// Lightweight test stub that avoids hardware GPIO and allows tests to
// control the cable connection state deterministically.
class SyncIn : public etl::observable<etl::observer<musin::timing::ClockEvent>,
                                      MAX_SYNC_IN_OBSERVERS> {
public:
  SyncIn(uint32_t /*sync_pin*/, uint32_t /*detect_pin*/) {
  }

  void update([[maybe_unused]] absolute_time_t now) {
  }

  [[nodiscard]] bool is_cable_connected() const {
    return cable_connected_;
  }

  // Test helper to control connection state
  void set_cable_connected(bool connected) {
    cable_connected_ = connected;
  }

private:
  bool cable_connected_ = false;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_SYNC_IN_H
