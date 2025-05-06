#include "musin/timing/tempo_handler.h"
#include "musin/timing/clock_event.h"
#include "musin/timing/tempo_event.h"

// Include headers for specific clock types if needed for identification
// #include "internal_clock.h"
// #include "midi_clock.h"
// #include "external_sync_clock.h"

namespace musin::timing {

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

void TempoHandler::notification(musin::timing::ClockEvent event) {
  // Only process and forward ticks if they come from the currently selected source
  if (event.source == current_source_) {
    musin::timing::TempoEvent tempo_tick_event;
    // Populate TempoEvent with timestamp or other data if needed later
    etl::observable<etl::observer<musin::timing::TempoEvent>,
                    MAX_TEMPO_OBSERVERS>::notify_observers(tempo_tick_event);
  }
}

} // namespace musin::timing
