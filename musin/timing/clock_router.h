#ifndef MUSIN_TIMING_CLOCK_ROUTER_H
#define MUSIN_TIMING_CLOCK_ROUTER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include <cstdint>

namespace musin::timing {

constexpr size_t MAX_CLOCK_ROUTER_OBSERVERS = 3;

/**
 * Selects the active raw 24 PPQN clock source and fans it out to observers.
 * Starts/stops internal clock, enables/disables MIDI forward echo,
 * and handles sync source management on source changes.
 */
class ClockRouter
    : public etl::observer<musin::timing::ClockEvent>,
      public etl::observable<etl::observer<musin::timing::ClockEvent>,
                             MAX_CLOCK_ROUTER_OBSERVERS> {
public:
  ClockRouter(InternalClock &internal_clock_ref,
              MidiClockProcessor &midi_clock_processor_ref, SyncIn &sync_in_ref,
              ClockSource initial_source = ClockSource::INTERNAL);

  void set_clock_source(ClockSource source);
  [[nodiscard]] ClockSource get_clock_source() const {
    return current_source_;
  }

  // From selected upstream source
  void notification(musin::timing::ClockEvent event) override;

  /**
   * @brief Trigger a manual resync event to all observers.
   */
  void trigger_resync();

  /**
   * @brief Emit a synthetic tick to all observers using the active source.
   * @param is_resync When true the tick is marked as a resync event.
   * @param anchor_phase Optional phase anchor to forward with the tick.
   */
  void emit_manual_tick(
      bool is_resync,
      uint8_t anchor_phase = musin::timing::ClockEvent::ANCHOR_PHASE_NONE);

private:
  void detach_current_source();
  void attach_source(ClockSource source);

  InternalClock &internal_clock_;
  MidiClockProcessor &midi_clock_processor_;
  SyncIn &sync_in_;
  ClockSource current_source_;
  bool initialized_ = false;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_CLOCK_ROUTER_H
