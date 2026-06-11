#include "musin/midi/midi_input_queue.h"

namespace musin::midi {

etl::queue_spsc_atomic<IncomingMidiMessage, MIDI_INPUT_QUEUE_SIZE,
                       etl::memory_model::MEMORY_MODEL_SMALL>
    midi_input_queue;

template <typename T>
bool enqueue_incoming_midi_message(const T &message_data) {
  if (!midi_input_queue.full()) {
    midi_input_queue.push(IncomingMidiMessage(message_data));
    return true;
  }
  return false; // Dropping new message if queue is full
}

// Explicit template instantiations
template bool enqueue_incoming_midi_message<NoteOnData>(const NoteOnData &);
template bool enqueue_incoming_midi_message<NoteOffData>(const NoteOffData &);
template bool
enqueue_incoming_midi_message<ControlChangeData>(const ControlChangeData &);
template bool
enqueue_incoming_midi_message<SystemRealtimeData>(const SystemRealtimeData &);

namespace {
etl::queue_spsc_atomic<sysex::Chunk, SYSEX_INPUT_QUEUE_SIZE,
                       etl::memory_model::MEMORY_MODEL_SMALL>
    sysex_input_queue;
} // namespace

bool enqueue_incoming_sysex_chunk(const uint8_t *data, size_t length) {
  // emplace constructs the chunk in the queue slot, avoiding a 2 KB stack
  // copy.
  return sysex_input_queue.emplace(data, length);
}

const sysex::Chunk *peek_incoming_sysex_chunk() {
  return sysex_input_queue.empty() ? nullptr : &sysex_input_queue.front();
}

void pop_incoming_sysex_chunk() {
  sysex_input_queue.pop();
}

bool dequeue_incoming_midi_message(IncomingMidiMessage &message) {
  if (midi_input_queue.empty()) {
    return false;
  }
  midi_input_queue.pop(message);
  return true;
}

} // namespace musin::midi
