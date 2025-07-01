#ifndef DRUM_NOTE_EVENT_QUEUE_H_
#define DRUM_NOTE_EVENT_QUEUE_H_

#include "drum/events.h"
#include "etl/queue.h"

namespace drum {

namespace NoteEventQueue {

void init();
bool push(const Events::NoteEvent &event);
bool pop(Events::NoteEvent &event);
bool is_empty();
bool is_full();

} // namespace NoteEventQueue
} // namespace drum

#endif // DRUM_NOTE_EVENT_QUEUE_H_
