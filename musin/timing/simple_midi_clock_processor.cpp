#include "simple_midi_clock_processor.h"

namespace musin::timing {

SimpleMidiClockProcessor::SimpleMidiClockProcessor() {
  last_tick_time_ = nil_time;
}

void SimpleMidiClockProcessor::on_midi_clock_tick_received() {
  last_tick_time_ = get_absolute_time();
  ClockEvent event{ClockSource::MIDI};
  notify_observers(event);
}

bool SimpleMidiClockProcessor::is_active() const {
  if (is_nil_time(last_tick_time_)) {
    return false;
  }
  return absolute_time_diff_us(last_tick_time_, get_absolute_time()) <
         MIDI_CLOCK_TIMEOUT_US;
}

} // namespace musin::timing
