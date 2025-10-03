#include "midi_clock_processor.h"
#include "musin/midi/midi_wrapper.h"
#include "pico/time.h"
#include <cstdio> // For printf (optional debugging)

namespace musin::timing {

MidiClockProcessor::MidiClockProcessor() {
  reset(); // Initialize all members
}

void MidiClockProcessor::on_midi_clock_tick_received() {
  absolute_time_t now = get_absolute_time();
  bool is_first_tick = is_nil_time(_last_raw_tick_time);

  if (!is_first_tick) {
    uint32_t current_interval_us =
        absolute_time_diff_us(_last_raw_tick_time, now);

    if (current_interval_us > MIDI_CLOCK_TIMEOUT_US) {
      // Timeout detected, clock might have stopped and restarted.
      reset();
      // Send resync event immediately.
      musin::timing::ClockEvent re_sync_tick_event{
          .source = musin::timing::ClockSource::MIDI, .is_resync = true};
      notify_observers(re_sync_tick_event);
    }
  }

  _last_raw_tick_time = now;

  // Forward MIDI clock tick to observers
  musin::timing::ClockEvent raw_tick_event{
      .source = musin::timing::ClockSource::MIDI, .is_resync = false};
  raw_tick_event.timestamp_us =
      static_cast<uint32_t>(to_us_since_boot(get_absolute_time()));
  notify_observers(raw_tick_event);

  if (forward_echo_enabled_) {
    MIDI::sendRealTime(midi::Clock);
  }
}

bool MidiClockProcessor::is_active() const {
  if (is_nil_time(_last_raw_tick_time)) {
    return false;
  }
  absolute_time_t now = get_absolute_time();
  return absolute_time_diff_us(_last_raw_tick_time, now) <
         MIDI_CLOCK_TIMEOUT_US;
}

void MidiClockProcessor::reset() {
  _last_raw_tick_time = nil_time;
}

} // namespace musin::timing
