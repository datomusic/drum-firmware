#ifndef MUSIN_MIDI_MIDI_MESSAGE_QUEUE_H
#define MUSIN_MIDI_MIDI_MESSAGE_QUEUE_H

#include "etl/array.h"
#include "etl/queue_spsc_atomic.h" // Using SPSC atomic queue
#include "midi_Defs.h"             // For ::midi::MidiType
#include "midi_wrapper.h"          // For MIDI::SysExMaxSize (from musin/midi/midi_wrapper.h)
#include <algorithm>               // For std::min, std::copy
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

struct NoteMessageData {
  uint8_t channel;
  uint8_t note;
  uint8_t velocity;
};

struct ControlChangeData {
  uint8_t channel;
  uint8_t controller;
  uint8_t value;
};

struct PitchBendData {
  uint8_t channel;
  int bend_value;
};

struct SystemRealtimeData {
  ::midi::MidiType type;
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

// Declare the global MIDI output queue (defined in .cpp)
// etl::queue_spsc_atomic is for a single producer and single consumer.
// The `enqueue_midi_message` function acts as the single point of entry for pushing to the queue.
// The `push` operation of etl::queue_spsc_atomic is ISR-safe if called from a single producer
// context. If `enqueue_midi_message` is called from multiple ISRs or an ISR and the main loop
// concurrently, external synchronization around the `midi_output_queue.push()` call within
// `enqueue_midi_message` might be needed if those calls could interleave at the instruction level
// for the push operation itself. However, the atomic nature of the queue's internal pointers should
// handle most cases correctly as long as `enqueue_midi_message` itself is treated as the "producer"
// action.
extern etl::queue_spsc_atomic<OutgoingMidiMessage, MIDI_QUEUE_SIZE,
                              etl::memory_model::MEMORY_MODEL_SMALL>
    midi_output_queue;

/**
 * @brief Enqueues a MIDI message to be sent.
 * This function is designed to be callable from various contexts.
 * @param message The MIDI message to enqueue.
 * @return True if the message was successfully enqueued, false if the queue was full.
 */
bool enqueue_midi_message(const OutgoingMidiMessage &message);

/**
 * @brief Processes the MIDI output queue.
 * This function should be called regularly from the main application loop (non-ISR context).
 * It dequeues messages and sends them, applying rate-limiting for non-real-time messages.
 */
void process_midi_output_queue();

} // namespace musin::midi

#endif // MUSIN_MIDI_MIDI_MESSAGE_QUEUE_H