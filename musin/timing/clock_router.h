#ifndef MUSIN_TIMING_CLOCK_ROUTER_H
#define MUSIN_TIMING_CLOCK_ROUTER_H

#include "etl/observer.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include <cstdint>

namespace musin::timing {

class SyncOut;

constexpr size_t MAX_CLOCK_ROUTER_OBSERVERS = 3;

/**
 * @brief Interface for receiving clock source change notifications.
 *
 * Implement this interface to be notified when ClockRouter switches between
 * INTERNAL, MIDI, or EXTERNAL_SYNC sources.
 */
class ISourceChangeListener {
public:
  virtual ~ISourceChangeListener() = default;
  virtual void on_clock_source_changed(ClockSource old_source,
                                       ClockSource new_source) = 0;
};

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

  // Control API
  void set_bpm(float bpm);
  void trigger_resync();
  void resync_sync_output();
  void set_sync_out(SyncOut *sync_out_ptr);

  // Auto source switching
  void update_auto_source_switching();
  void set_auto_switching_enabled(bool enabled);

  // Source change notification
  void set_source_change_listener(ISourceChangeListener *listener);

  // From selected upstream source
  void notification(musin::timing::ClockEvent event) override;

private:
  void detach_current_source();
  void attach_source(ClockSource source);

  InternalClock &internal_clock_;
  MidiClockProcessor &midi_clock_processor_;
  SyncIn &sync_in_;
  ClockSource current_source_;
  bool initialized_ = false;
  bool auto_switching_enabled_ = true;
  SyncOut *sync_out_ = nullptr;
  ISourceChangeListener *source_change_listener_ = nullptr;
  bool awaiting_first_tick_after_switch_ = false;
};

} // namespace musin::timing

#endif // MUSIN_TIMING_CLOCK_ROUTER_H
