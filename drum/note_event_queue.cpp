#include "drum/note_event_queue.h"
#include "drum/config.h" // For queue size

namespace drum {

namespace {
constexpr size_t NOTE_EVENT_QUEUE_SIZE = 32;
etl::queue<Events::NoteEvent, NOTE_EVENT_QUEUE_SIZE> note_event_queue;
} // namespace

namespace NoteEventQueue {

void init() {
  note_event_queue.clear();
}

bool push(const Events::NoteEvent &event) {
  if (note_event_queue.full()) {
    // In a real scenario, you might want to log this overrun.
    // For now, we just drop the event.
    return false;
  }
  note_event_queue.push(event);
  return true;
}

bool pop(Events::NoteEvent &event) {
  if (note_event_queue.empty()) {
    return false;
  }
  event = note_event_queue.front();
  note_event_queue.pop();
  return true;
}

bool is_empty() {
  return note_event_queue.empty();
}

bool is_full() {
  return note_event_queue.full();
}

} // namespace NoteEventQueue
} // namespace drum
