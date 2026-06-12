#ifndef MUSIN_MIDI_MIDI_INPUT_QUEUE_H
#define MUSIN_MIDI_MIDI_INPUT_QUEUE_H

#include "musin/midi/sysex_chunk.h"
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

using IncomingMidiMessage = etl::variant<NoteOnData, NoteOffData,
                                         ControlChangeData, SystemRealtimeData>;

// SysEx chunks own a MAX_PAYLOAD_SIZE buffer each, so they get a dedicated
// shallow queue instead of inflating every slot of the main message queue.
// The transfer protocol is ACK-paced, so more than two in flight is abnormal.
constexpr size_t SYSEX_INPUT_QUEUE_SIZE = 2;

extern etl::queue_spsc_atomic<IncomingMidiMessage, MIDI_INPUT_QUEUE_SIZE,
                              etl::memory_model::MEMORY_MODEL_SMALL>
    midi_input_queue;

template <typename T> bool enqueue_incoming_midi_message(const T &message_data);

bool dequeue_incoming_midi_message(IncomingMidiMessage &message);

/**
 * @brief Copies a SysEx payload into the sysex queue. Drops it if full.
 */
bool enqueue_incoming_sysex_chunk(const uint8_t *data, size_t length);

/**
 * @brief Returns the oldest queued chunk, or nullptr if the queue is empty.
 * Call pop_incoming_sysex_chunk() after processing it.
 */
const sysex::Chunk *peek_incoming_sysex_chunk();

void pop_incoming_sysex_chunk();

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_INPUT_QUEUE_H
