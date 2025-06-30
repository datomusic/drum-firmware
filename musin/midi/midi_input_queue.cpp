#include "musin/midi/midi_input_queue.h"

namespace musin::midi {

etl::queue_spsc_atomic<IncomingMidiMessage, MIDI_INPUT_QUEUE_SIZE,
                       etl::memory_model::MEMORY_MODEL_SMALL>
    midi_input_queue;

bool enqueue_incoming_midi_message(const IncomingMidiMessage &message) {
  if (!midi_input_queue.full()) {
    midi_input_queue.push(message);
    return true;
  }
  return false; // Dropping new message if queue is full
}

bool dequeue_incoming_midi_message(IncomingMidiMessage &message) {
  if (midi_input_queue.empty()) {
    return false;
  }
  midi_input_queue.pop(message);
  return true;
}

} // namespace musin::midi
