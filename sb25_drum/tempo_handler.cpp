#include "tempo_handler.h"

// Include headers for specific clock types if needed for identification
// #include "internal_clock.h"
// #include "midi_clock.h"
// #include "external_sync_clock.h"

#include <cstdio> // For printf (debugging)

namespace Tempo {

TempoHandler::TempoHandler(ClockSource initial_source) : current_source_(initial_source) {
  printf("TempoHandler: Initialized. Default source: %u\n",
         static_cast<uint8_t>(current_source_));
  // If managing clock instances internally or needing references:
  // Initialize clock references/pointers here, potentially passed via constructor.
}

void TempoHandler::set_clock_source(ClockSource source) {
  if (source != current_source_) {
    printf("TempoHandler: Changing clock source from %u to %u\n",
           static_cast<uint8_t>(current_source_), static_cast<uint8_t>(source));
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
  // This is where the core logic resides.
  // 1. Identify the source of the event.
  //    - This might involve adding a 'source' field to ClockEvent.
  //    - Or, if TempoHandler registers itself only with the active clock,
  //      any notification received can be assumed to be from the active source.
  // 2. Check if the event source matches the 'current_source_'.
  // 3. If it matches, process the tick (e.g., calculate BPM, update state).
  // 4. Create a TempoEvent.
  // 5. Notify observers (like TempoMultiplier).

  // --- Placeholder Implementation ---
  // Assuming any received event is potentially valid for the current source
  // (Requires refinement based on how clocks notify and how TempoHandler subscribes)

  // printf("TempoHandler: Received ClockEvent. Current source: %u\n",
  // static_cast<uint8_t>(current_source_));

  // TODO: Add logic to determine if this event *actually* came from the
  //       'current_source_'. This depends heavily on the Clock architecture.
  bool event_is_from_active_source = true; // Placeholder assumption

  if (event_is_from_active_source) {
    // Process the tick if necessary (e.g., update internal BPM calculation)

    // Create the event to send downstream
    Tempo::TempoEvent tempo_tick_event;
    // Populate tempo_tick_event fields (e.g., timestamp, bpm) if needed

    // Notify observers (e.g., TempoMultiplier)
    etl::observable<etl::observer<TempoEvent>, MAX_TEMPO_OBSERVERS>::notify_observers(
        tempo_tick_event);
    // printf("TempoHandler: Notified observers with TempoEvent.\n");
  } else {
    // printf("TempoHandler: Ignored ClockEvent from inactive source.\n");
  }
}

} // namespace Tempo
