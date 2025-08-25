#include "pico_clock.h"

namespace musin {
namespace timing {

PicoClock::PicoClock(float initial_bpm)
    : internal_clock_(initial_bpm), tempo_handler_(internal_clock_) {
  internal_clock_.add_observer(tempo_handler_);
  tempo_handler_.add_observer(*this);
}

void PicoClock::start() {
  internal_clock_.start();
}

void PicoClock::stop() {
  internal_clock_.stop();
}

bool PicoClock::is_running() const {
  return internal_clock_.is_running();
}

void PicoClock::set_bpm(float bpm) {
  internal_clock_.set_bpm(bpm);
}

float PicoClock::get_bpm() const {
  return internal_clock_.get_bpm();
}

void PicoClock::notification(ClockEvent event) {
  // Forward TempoEvents to our own observers
  this->notify_observers(TempoEvent{event.type});
}

} // namespace timing
} // namespace musin