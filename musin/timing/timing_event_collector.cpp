#include "musin/timing/timing_event_collector.h"

namespace musin::timing {

void TimingEventCollector::notification(musin::timing::ClockEvent event) {
  absolute_time_t now = get_absolute_time();
  switch (event.source) {
  case ClockSource::EXTERNAL_SYNC:
    _last_sync_ref_tick_time.store(now, std::memory_order_relaxed);
    break;
  case ClockSource::MIDI:
    _last_midi_ref_tick_time.store(now, std::memory_order_relaxed);
    _midi_tick_counter.fetch_add(1, std::memory_order_relaxed);
    break;
  case ClockSource::INTERNAL:
    // The collector does not listen to the internal clock.
    break;
  }
}

absolute_time_t TimingEventCollector::get_and_reset_last_sync_tick_time() {
  return _last_sync_ref_tick_time.exchange(nil_time, std::memory_order_relaxed);
}

absolute_time_t TimingEventCollector::get_and_reset_last_midi_tick_time() {
  return _last_midi_ref_tick_time.exchange(nil_time, std::memory_order_relaxed);
}

uint32_t TimingEventCollector::get_and_reset_midi_tick_count() {
  return _midi_tick_counter.exchange(0, std::memory_order_relaxed);
}

} // namespace musin::timing
