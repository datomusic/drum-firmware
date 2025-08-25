#ifndef MUSIN_TIMING_TIMING_EVENT_COLLECTOR_H
#define MUSIN_TIMING_TIMING_EVENT_COLLECTOR_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "pico/time.h"
#include <atomic>
#include <cstdint>

namespace musin::timing {

class TimingEventCollector : public etl::observer<musin::timing::ClockEvent> {
public:
  TimingEventCollector() = default;

  void notification(musin::timing::ClockEvent event) override;

  // Atomically reads the last tick time and resets it to nil.
  absolute_time_t get_and_reset_last_sync_tick_time();
  absolute_time_t get_and_reset_last_midi_tick_time();

  // Atomically reads the number of ticks and resets the counter.
  uint32_t get_and_reset_midi_tick_count();

private:
  std::atomic<absolute_time_t> _last_sync_ref_tick_time{nil_time};
  std::atomic<absolute_time_t> _last_midi_ref_tick_time{nil_time};
  std::atomic<uint32_t> _midi_tick_counter{0};
};

} // namespace musin::timing

#endif // MUSIN_TIMING_TIMING_EVENT_COLLECTOR_H
