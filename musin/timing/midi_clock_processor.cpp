#include "midi_clock_processor.h"
#include <algorithm> // For std::fill
#include <cstdio>    // For printf (optional debugging)

namespace musin::timing {

MidiClockProcessor::MidiClockProcessor() {
  reset(); // Initialize all members
}

void MidiClockProcessor::on_midi_clock_tick_received() {
  absolute_time_t now = get_absolute_time();

  if (!is_nil_time(_last_raw_tick_time)) {
    uint32_t current_interval_us =
        absolute_time_diff_us(_last_raw_tick_time, now);

    if (current_interval_us > MIDI_CLOCK_TIMEOUT_US) {
      // Timeout detected, clock might have stopped and restarted. Reset.
      // printf("MCP: MIDI Clock timeout, resetting.\n");
      reset();
      // Treat this tick as the first tick after a reset, so it will just update
      // _last_raw_tick_time
    } else if (current_interval_us <
               1000) { // Less than 1ms, likely noise or error (max 60000 BPM)
      // printf("MCP: Interval too short (%u us), ignoring.\n",
      // current_interval_us);
      _last_raw_tick_time =
          now; // Update time to prevent rapid re-triggering of this path
      return;
    } else {
      // Store valid interval
      _raw_tick_intervals_us[_interval_history_index] = current_interval_us;
      _interval_history_index =
          (_interval_history_index + 1) % MIDI_CLOCK_INTERVAL_HISTORY_SIZE;
      if (_interval_history_count < MIDI_CLOCK_INTERVAL_HISTORY_SIZE) {
        _interval_history_count++;
      }
      update_derived_bpm();
    }
  } else {
    // This is the first tick received after construction or a reset.
    // printf("MCP: First MIDI Clock tick received.\n");
    // No interval to calculate yet, but we start the timing for the next stable
    // tick. If we have a valid derived BPM (e.g. from a previous session before
    // reset), we might want to use it, but typically after reset, BPM is 0.
  }

  _last_raw_tick_time = now;

  // If we have a derived BPM, generate a stable tick.
  // The stable tick generation is phase-aligned by scheduling it relative to
  // the first tick that allowed a BPM calculation, or by adjusting phase based
  // on incoming ticks. A simpler approach for now: generate a tick if BPM is
  // valid. The `_next_stable_tick_target_time` helps in generating ticks at a
  // stable rate.

  if (_derived_bpm > 0.0f) {
    if (is_nil_time(_next_stable_tick_target_time)) {
      // This is the first opportunity to generate a stable tick after BPM is
      // established. Schedule it based on the current time and average
      // interval.
      _next_stable_tick_target_time = delayed_by_us(now, _average_interval_us);
      musin::timing::ClockEvent stable_tick_event{
          musin::timing::ClockSource::MIDI};
      notify_observers(stable_tick_event);
      // printf("MCP: Sent first stable tick. Next in %u us\n",
      // _average_interval_us);
    } else {
      // Check if it's time for the next scheduled stable tick
      // Using <= 0 for time_reached might be problematic if `now` is slightly
      // before. A small window or checking if `now` is "close enough" and past
      // might be better, but for simplicity, we use a direct comparison.
      if (time_reached(_next_stable_tick_target_time)) {
        musin::timing::ClockEvent stable_tick_event{
            musin::timing::ClockSource::MIDI};
        notify_observers(stable_tick_event);
        // Schedule the next one from the *target* time to avoid drift
        _next_stable_tick_target_time =
            delayed_by_us(_next_stable_tick_target_time, _average_interval_us);
        // printf("MCP: Sent stable tick. Next in %u us\n",
        // _average_interval_us);
      }
    }
  }
}

void MidiClockProcessor::update_derived_bpm() {
  if (_interval_history_count < MIDI_CLOCK_INTERVAL_HISTORY_SIZE / 4 &&
      _interval_history_count < 2) {
    // Require at least a few samples, or quarter of history, to start deriving
    // BPM
    _derived_bpm = 0.0f;
    _average_interval_us = 0;
    _next_stable_tick_target_time =
        nil_time; // Reset target if BPM is not stable
    return;
  }

  uint64_t sum_intervals = 0;
  for (size_t i = 0; i < _interval_history_count; ++i) {
    sum_intervals += _raw_tick_intervals_us[i];
  }

  if (_interval_history_count > 0) {
    _average_interval_us = sum_intervals / _interval_history_count;
  } else {
    _average_interval_us = 0;
  }

  if (_average_interval_us > 0) {
    // MIDI clock is 24 PPQN
    _derived_bpm = (60.0f * 1000000.0f) /
                   (static_cast<float>(_average_interval_us) * 24.0f);
    // printf("MCP: Derived BPM: %.2f, Avg Interval: %u us, History Count:
    // %u\n", _derived_bpm, _average_interval_us, _interval_history_count);
  } else {
    _derived_bpm = 0.0f;
    _next_stable_tick_target_time =
        nil_time; // Reset target if BPM is not stable
  }
}

float MidiClockProcessor::get_derived_bpm() const {
  if (is_nil_time(_last_raw_tick_time)) {
    return 0.0f;
  }
  return _derived_bpm;
}

void MidiClockProcessor::reset() {
  // printf("MCP: Resetting MidiClockProcessor state.\n");
  std::fill(_raw_tick_intervals_us.begin(), _raw_tick_intervals_us.end(), 0U);
  _interval_history_index = 0;
  _interval_history_count = 0;
  _last_raw_tick_time = nil_time;
  _derived_bpm = 0.0f;
  _average_interval_us = 0;
  _next_stable_tick_target_time = nil_time;
}

} // namespace musin::timing
