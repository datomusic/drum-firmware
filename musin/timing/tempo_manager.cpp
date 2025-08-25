#include "musin/timing/tempo_manager.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/timing_event_collector.h"

namespace musin::timing {

TempoManager::TempoManager(InternalClock &internal_clock,
                           MidiClockProcessor &midi_clock_processor,
                           SyncIn &sync_in,
                           TimingEventCollector &event_collector)
    : _internal_clock(internal_clock),
      _midi_clock_processor(midi_clock_processor), _sync_in(sync_in),
      _event_collector(event_collector) {
  // Start the internal clock by default
  _internal_clock.start();
}

void TempoManager::set_clock_source(ClockSource source) {
  if (source == _current_source) {
    return;
  }

  // Reset state when switching away from a source
  if (_current_source == ClockSource::MIDI) {
    _midi_clock_processor.reset();
  }

  _current_source = source;
  _last_bpm_calculation_time = nil_time; // Reset timer when source changes
}

void TempoManager::update(absolute_time_t now) {
  // --- 1. Clock Source Selection ---
  if (_sync_in.is_cable_connected()) {
    set_clock_source(ClockSource::EXTERNAL_SYNC);
  } else if (_midi_clock_processor.get_derived_bpm() > 0.0f) {
    set_clock_source(ClockSource::MIDI);
  } else {
    set_clock_source(ClockSource::INTERNAL);
  }

  // --- 2. Process based on selected source ---
  if (_current_source == ClockSource::EXTERNAL_SYNC) {
    update_sync_source(now);
  } else if (_current_source == ClockSource::MIDI) {
    update_midi_source(now);
  }
  // If INTERNAL, we do nothing; it just runs at its set tempo.
}

void TempoManager::update_sync_source(absolute_time_t now) {
  absolute_time_t last_tick_time =
      _event_collector.get_and_reset_last_sync_tick_time();

  if (!is_nil_time(last_tick_time)) {
    // A new tick has arrived.
    _internal_clock.resynchronize();

    if (!is_nil_time(_last_bpm_calculation_time)) {
      int64_t interval_us =
          absolute_time_diff_us(_last_bpm_calculation_time, last_tick_time);
      if (interval_us > 0) {
        // Sync In is 2 PPQN
        float new_bpm = (60.0f * 1000000.0f) / (float(interval_us) * 2.0f);
        _internal_clock.set_bpm(new_bpm);
      }
    }
    _last_bpm_calculation_time = last_tick_time;
  }
}

void TempoManager::update_midi_source(absolute_time_t now) {
  uint32_t tick_count = _event_collector.get_and_reset_midi_tick_count();

  if (tick_count > 0) {
    if (!is_nil_time(_last_bpm_calculation_time)) {
      int64_t interval_us =
          absolute_time_diff_us(_last_bpm_calculation_time, now);
      if (interval_us > 0) {
        // MIDI is 24 PPQN
        float ticks_per_second =
            float(tick_count * 1000000) / float(interval_us);
        float new_bpm = (ticks_per_second / 24.0f) * 60.0f;
        _internal_clock.set_bpm(new_bpm);
      }
    }
    _last_bpm_calculation_time = now;
  }
}

void TempoManager::set_bpm(float bpm) {
  if (_current_source == ClockSource::INTERNAL) {
    _internal_clock.set_bpm(bpm);
  }
}

} // namespace musin::timing
