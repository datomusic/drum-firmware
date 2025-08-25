#ifndef MUSIN_TIMING_TEMPO_MANAGER_H
#define MUSIN_TIMING_TEMPO_MANAGER_H

#include "musin/timing/clock_event.h"
#include "pico/time.h"
#include <cstdint>

namespace musin::timing {

// Forward declarations
class InternalClock;
class MidiClockProcessor;
class SyncIn;
class TimingEventCollector;

class TempoManager {
public:
  explicit TempoManager(InternalClock &internal_clock,
                        MidiClockProcessor &midi_clock_processor,
                        SyncIn &sync_in, TimingEventCollector &event_collector);

  void update(absolute_time_t now);
  void set_bpm(float bpm);

private:
  void set_clock_source(ClockSource source);
  void update_sync_source(absolute_time_t now);
  void update_midi_source(absolute_time_t now);

  InternalClock &_internal_clock;
  MidiClockProcessor &_midi_clock_processor;
  SyncIn &_sync_in;
  TimingEventCollector &_event_collector;

  ClockSource _current_source = ClockSource::INTERNAL;
  absolute_time_t _last_bpm_calculation_time = nil_time;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_TEMPO_MANAGER_H
