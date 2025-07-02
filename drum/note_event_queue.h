#ifndef DRUM_NOTE_EVENT_QUEUE_H_
#define DRUM_NOTE_EVENT_QUEUE_H_

#include "drum/events.h"
#include "etl/queue.h"

namespace drum {

class NoteEventQueue {
public:
  NoteEventQueue() {
    queue_.clear();
  }

  bool push(const Events::NoteEvent &event) {
    if (queue_.full()) {
      // In a real scenario, you might want to log this overrun.
      // For now, we just drop the event.
      return false;
    }
    queue_.push(event);
    return true;
  }

  bool pop(Events::NoteEvent &event) {
    if (queue_.empty()) {
      return false;
    }
    event = queue_.front();
    queue_.pop();
    return true;
  }

  bool is_empty() const {
    return queue_.empty();
  }

  bool is_full() const {
    return queue_.full();
  }

private:
  static constexpr size_t NOTE_EVENT_QUEUE_SIZE = 32;
  etl::queue<Events::NoteEvent, NOTE_EVENT_QUEUE_SIZE> queue_;
};

} // namespace drum

#endif // DRUM_NOTE_EVENT_QUEUE_H_
