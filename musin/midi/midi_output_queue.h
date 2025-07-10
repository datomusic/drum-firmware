#ifndef MUSIN_MIDI_MIDI_MESSAGE_QUEUE_H
#define MUSIN_MIDI_MIDI_MESSAGE_QUEUE_H

#include "etl/array.h"
#include "etl/queue.h" // Using a standard queue with external locking
#include "midi_common.h"
#include "midi_wrapper.h" // For MIDI::SysExMaxSize (from musin/midi/midi_wrapper.h)
#include "musin/hal/logger.h" // Include logger header
#include <algorithm>      // For std::min, std::copy
#include <cstdint>

namespace musin::midi {

// Configuration for the MIDI queue
constexpr size_t MIDI_QUEUE_SIZE = 64; // Max number of MIDI messages in the queue

enum class MidiMessageType : uint8_t {
  NOTE_ON,
  NOTE_OFF,
  CONTROL_CHANGE,
  PITCH_BEND,
  SYSTEM_REALTIME,
  SYSTEM_EXCLUSIVE
};

struct SystemExclusiveData {
  // Uses MIDI::SysExMaxSize from musin/midi/midi_wrapper.h (which is 128)
  etl::array<uint8_t, MIDI::SysExMaxSize> data_buffer;
  unsigned length;
};

struct OutgoingMidiMessage {
  MidiMessageType type;
  union MessageUnion {
    NoteMessageData note_message;
    ControlChangeData control_change_message;
    PitchBendData pitch_bend_message;
    SystemRealtimeData system_realtime_message;
    SystemExclusiveData system_exclusive_message;

    // Default constructor for the union.
    // Members will be initialized by the OutgoingMidiMessage constructors.
    MessageUnion() {
    }
  } data;

  // Constructors
  OutgoingMidiMessage() : type(MidiMessageType::SYSTEM_REALTIME) {
    // Default constructor, ensure data is initialized if needed for a default message
    data.system_realtime_message.type = ::midi::InvalidType;
  }

  OutgoingMidiMessage(::midi::MidiType rt_type) : type(MidiMessageType::SYSTEM_REALTIME) {
    data.system_realtime_message.type = rt_type;
  }

  OutgoingMidiMessage(uint8_t ch, uint8_t n, uint8_t vel, bool is_on)
      : type(is_on ? MidiMessageType::NOTE_ON : MidiMessageType::NOTE_OFF) {
    data.note_message.channel = ch;
    data.note_message.note = n;
    data.note_message.velocity = vel;
  }

  OutgoingMidiMessage(uint8_t ch, uint8_t ctrl, uint8_t val)
      : type(MidiMessageType::CONTROL_CHANGE) {
    data.control_change_message.channel = ch;
    data.control_change_message.controller = ctrl;
    data.control_change_message.value = val;
  }

  OutgoingMidiMessage(uint8_t ch, int pb_val) : type(MidiMessageType::PITCH_BEND) {
    data.pitch_bend_message.channel = ch;
    data.pitch_bend_message.bend_value = pb_val;
  }

  OutgoingMidiMessage(const uint8_t *sysex_payload, unsigned len)
      : type(MidiMessageType::SYSTEM_EXCLUSIVE) {
    // Ensure length does not exceed the buffer size defined by MIDI::SysExMaxSize from
    // midi_wrapper.h
    data.system_exclusive_message.length = std::min(len, static_cast<unsigned>(MIDI::SysExMaxSize));
    if (sysex_payload != nullptr) { // Check for null payload
      std::copy(sysex_payload, sysex_payload + data.system_exclusive_message.length,
                data.system_exclusive_message.data_buffer.begin());
    } else {
      // If payload is null, ensure length is 0 to avoid copying garbage
      data.system_exclusive_message.length = 0;
    }
  }
};

extern etl::queue<OutgoingMidiMessage, MIDI_QUEUE_SIZE> midi_output_queue;

bool enqueue_midi_message(const OutgoingMidiMessage &message, musin::Logger &logger);

void process_midi_output_queue(musin::Logger &logger);

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_MESSAGE_QUEUE_H
