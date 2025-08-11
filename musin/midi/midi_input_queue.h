#ifndef MUSIN_MIDI_MIDI_INPUT_QUEUE_H
#define MUSIN_MIDI_MIDI_INPUT_QUEUE_H

#include "drum/sysex/chunk.h"
#include "etl/queue_spsc_atomic.h"
#include "etl/span.h"
#include "etl/variant.h"
#include "midi_common.h"
#include <cstdint>

namespace musin::midi {

constexpr size_t MIDI_INPUT_QUEUE_SIZE = 64;

struct NoteOnData {
  uint8_t channel;
  uint8_t note;
  uint8_t velocity;
};

struct NoteOffData {
  uint8_t channel;
  uint8_t note;
  uint8_t velocity;
};

using IncomingMidiMessage =
    etl::variant<NoteOnData, NoteOffData, ControlChangeData, SystemRealtimeData,
                 sysex::Chunk>;

extern etl::queue_spsc_atomic<IncomingMidiMessage, MIDI_INPUT_QUEUE_SIZE,
                              etl::memory_model::MEMORY_MODEL_SMALL>
    midi_input_queue;

template <typename T> bool enqueue_incoming_midi_message(const T &message_data);

bool dequeue_incoming_midi_message(IncomingMidiMessage &message);

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_INPUT_QUEUE_H
