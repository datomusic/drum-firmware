#ifndef PICO_CLOCK_H
#define PICO_CLOCK_H

#include "etl/observer.h"
#include "i_clock.h"
#include "internal_clock.h"
#include "tempo_handler.h"

namespace musin {
namespace timing {

class PicoClock : public I_Clock, public etl::observer<ClockEvent> {
public:
  explicit PicoClock(float initial_bpm = 120.0f);

  void start() override;
  void stop() override;
  bool is_running() const override;
  void set_bpm(float bpm) override;
  float get_bpm() const override;

  // etl::observer<ClockEvent> implementation
  void notification(ClockEvent event) override;

private:
  InternalClock internal_clock_;
  TempoHandler tempo_handler_;
};

} // namespace timing
} // namespace musin

#endif // PICO_CLOCK_H