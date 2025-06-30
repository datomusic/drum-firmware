#ifndef MUSIN_MIDI_MIDI_INPUT_QUEUE_H
#define MUSIN_MIDI_MIDI_INPUT_QUEUE_H

#include "etl/queue_spsc_atomic.h"
#include "drum/sysex/chunk.h"
#include "midi_common.h"
#include <cstdint>

namespace musin::midi {

constexpr size_t MIDI_INPUT_QUEUE_SIZE = 64;

enum class IncomingMidiMessageType : uint8_t {
  NOTE_ON,
  NOTE_OFF,
  CONTROL_CHANGE,
  SYSTEM_REALTIME,
  SYSTEM_EXCLUSIVE
};

struct IncomingMidiMessage {
  IncomingMidiMessageType type;
  union MessageUnion {
    NoteMessageData note_message;
    ControlChangeData control_change_message;
    SystemRealtimeData system_realtime_message;
    sysex::Chunk system_exclusive_message;

    MessageUnion() {}
    ~MessageUnion() {} // Trivial destructor
  } data;

  IncomingMidiMessage() : type(IncomingMidiMessageType::SYSTEM_REALTIME) {
    data.system_realtime_message.type = ::midi::InvalidType;
  }

  IncomingMidiMessage(::midi::MidiType rt_type)
      : type(IncomingMidiMessageType::SYSTEM_REALTIME) {
    data.system_realtime_message.type = rt_type;
  }

  IncomingMidiMessage(uint8_t ch, uint8_t n, uint8_t vel, bool is_on)
      : type(is_on ? IncomingMidiMessageType::NOTE_ON : IncomingMidiMessageType::NOTE_OFF) {
    data.note_message.channel = ch;
    data.note_message.note = n;
    data.note_message.velocity = vel;
  }

  IncomingMidiMessage(uint8_t ch, uint8_t ctrl, uint8_t val)
      : type(IncomingMidiMessageType::CONTROL_CHANGE) {
    data.control_change_message.channel = ch;
    data.control_change_message.controller = ctrl;
    data.control_change_message.value = val;
  }

  IncomingMidiMessage(const sysex::Chunk &sysex_chunk)
      : type(IncomingMidiMessageType::SYSTEM_EXCLUSIVE) {
    new (&data.system_exclusive_message) sysex::Chunk(sysex_chunk);
  }

  // Copy constructor
  IncomingMidiMessage(const IncomingMidiMessage &other) : type(other.type) {
    switch (type) {
    case IncomingMidiMessageType::NOTE_ON:
    case IncomingMidiMessageType::NOTE_OFF:
      data.note_message = other.data.note_message;
      break;
    case IncomingMidiMessageType::CONTROL_CHANGE:
      data.control_change_message = other.data.control_change_message;
      break;
    case IncomingMidiMessageType::SYSTEM_REALTIME:
      data.system_realtime_message = other.data.system_realtime_message;
      break;
    case IncomingMidiMessageType::SYSTEM_EXCLUSIVE:
      new (&data.system_exclusive_message) sysex::Chunk(other.data.system_exclusive_message);
      break;
    }
  }

  // Copy assignment operator
  IncomingMidiMessage &operator=(const IncomingMidiMessage &other) {
    if (this != &other) {
      if (type == IncomingMidiMessageType::SYSTEM_EXCLUSIVE) {
        data.system_exclusive_message.~Chunk();
      }
      type = other.type;
      switch (type) {
      case IncomingMidiMessageType::NOTE_ON:
      case IncomingMidiMessageType::NOTE_OFF:
        data.note_message = other.data.note_message;
        break;
      case IncomingMidiMessageType::CONTROL_CHANGE:
        data.control_change_message = other.data.control_change_message;
        break;
      case IncomingMidiMessageType::SYSTEM_REALTIME:
        data.system_realtime_message = other.data.system_realtime_message;
        break;
      case IncomingMidiMessageType::SYSTEM_EXCLUSIVE:
        new (&data.system_exclusive_message) sysex::Chunk(other.data.system_exclusive_message);
        break;
      }
    }
    return *this;
  }

  ~IncomingMidiMessage() {
    if (type == IncomingMidiMessageType::SYSTEM_EXCLUSIVE) {
      data.system_exclusive_message.~Chunk();
    }
  }
};

extern etl::queue_spsc_atomic<IncomingMidiMessage, MIDI_INPUT_QUEUE_SIZE,
                              etl::memory_model::MEMORY_MODEL_SMALL>
    midi_input_queue;

bool enqueue_incoming_midi_message(const IncomingMidiMessage &message);

bool dequeue_incoming_midi_message(IncomingMidiMessage &message);

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_INPUT_QUEUE_H
