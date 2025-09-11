#ifndef MUSIN_TIMING_MIDI_CLOCK_OUT_H
#define MUSIN_TIMING_MIDI_CLOCK_OUT_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_handler.h"

namespace musin::timing {

/**
 * Observes raw 24 PPQN ClockEvent and emits MIDI realtime Clock ticks
 * according to playback policy and active source.
 */
class MidiClockOut : public etl::observer<musin::timing::ClockEvent> {
public:
  MidiClockOut(musin::timing::TempoHandler &tempo_handler_ref,
               bool send_when_stopped_as_master);

  void notification(musin::timing::ClockEvent event) override;

private:
  musin::timing::TempoHandler &tempo_handler_;
  const bool send_when_stopped_;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_MIDI_CLOCK_OUT_H

