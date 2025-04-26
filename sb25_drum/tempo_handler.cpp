#include "tempo_handler.h"

// Include headers for specific clock types if needed for identification
// #include "internal_clock.h"
// #include "midi_clock.h"
// #include "external_sync_clock.h"


namespace Tempo {

TempoHandler::TempoHandler(ClockSource initial_source) : current_source_(initial_source) {
  // If managing clock instances internally or needing references:
  // Initialize clock references/pointers here, potentially passed via constructor.
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source != current_source_) {
    current_source_ = source;
    // Add logic here if switching sources requires actions like:
    // - Detaching from the old source's observable notifications
    // - Attaching to the new source's observable notifications
    // - Resetting tempo calculation state
  }
}

ClockSource TempoHandler::get_clock_source() const {
  return current_source_;
}

void TempoHandler::notification(const Clock::ClockEvent &event) {
  // TODO: Add logic to determine if this event *actually* came from the
  //       'current_source_'. This depends heavily on the Clock architecture.
  //       For now, assume any received event is from the active source if
  //       TempoHandler only subscribes to the active clock.
  bool event_is_from_active_source = true; // Placeholder assumption

  if (event_is_from_active_source) {
    // Process the tick if necessary (e.g., update internal BPM calculation)

    Tempo::TempoEvent tempo_tick_event;
    // Populate tempo_tick_event fields (e.g., timestamp, bpm) if needed

    etl::observable<etl::observer<TempoEvent>, MAX_TEMPO_OBSERVERS>::notify_observers(
        tempo_tick_event);
  }
}

} // namespace Tempo
