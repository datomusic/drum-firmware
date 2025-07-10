#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::internal actual send functions
#include "pico/time.h" // For RP2040 specific timing (get_absolute_time, absolute_time_diff_us, is_nil_time)
#include <cstdio> // For printf

namespace musin::midi {

// Define the global queue instance
etl::queue_spsc_atomic<OutgoingMidiMessage, MIDI_QUEUE_SIZE, etl::memory_model::MEMORY_MODEL_SMALL>
    midi_output_queue;

bool enqueue_midi_message(const OutgoingMidiMessage &message, musin::Logger &logger) {
  if (!midi_output_queue.full()) {
    midi_output_queue.push(message);
    return true;
  }
  // Log queue full error
  logger.debug("MIDI queue full - message dropped");
  return false;
}

// Rate limiting for non-real-time messages
constexpr uint32_t MIN_INTERVAL_US_NON_REALTIME = 960; // 3125 bytes per second at 3 bytes each
static absolute_time_t last_non_realtime_send_time = nil_time;

void process_midi_output_queue(musin::Logger &logger) {
  if (midi_output_queue.empty()) {
    return;
  }

  // Peek at the message to decide if we can send it now (due to rate limiting)
  OutgoingMidiMessage &message_to_process = midi_output_queue.front();

  bool can_send = false;
  if (message_to_process.type == MidiMessageType::SYSTEM_REALTIME) {
    can_send = true; // Real-time messages are sent immediately
  } else {
    // Check rate limit for non-real-time messages
    if (is_nil_time(last_non_realtime_send_time) ||
        absolute_time_diff_us(last_non_realtime_send_time, get_absolute_time()) >=
            MIN_INTERVAL_US_NON_REALTIME) {
      can_send = true;
    }
  }

  if (!can_send) {
    // Log deferred message with type and time since last send
    int64_t time_since_last =
        is_nil_time(last_non_realtime_send_time)
            ? -1
            : absolute_time_diff_us(last_non_realtime_send_time, get_absolute_time());
    logger.log(LogLevel::DEBUG, "Deferred MIDI type",
               static_cast<int32_t>(message_to_process.type));
    logger.log(LogLevel::DEBUG, "Time since last send (us)", static_cast<int32_t>(time_since_last));
  }

  if (can_send) {
    // Make a copy and pop before sending, in case sending itself triggers another enqueue.
    OutgoingMidiMessage message = message_to_process; // Copy the message
    midi_output_queue.pop();                          // Remove from queue

    // Log message processing
    logger.log(LogLevel::DEBUG, "Processing MIDI type", static_cast<int32_t>(message.type));

    switch (message.type) {
    case MidiMessageType::NOTE_ON:
      MIDI::internal::_sendNoteOn_actual(message.data.note_message.channel,
                                         message.data.note_message.note,
                                         message.data.note_message.velocity);
      last_non_realtime_send_time = get_absolute_time();
      break;
    case MidiMessageType::NOTE_OFF:
      MIDI::internal::_sendNoteOff_actual(message.data.note_message.channel,
                                          message.data.note_message.note,
                                          message.data.note_message.velocity);
      last_non_realtime_send_time = get_absolute_time();
      break;
    case MidiMessageType::CONTROL_CHANGE:
      MIDI::internal::_sendControlChange_actual(message.data.control_change_message.channel,
                                                message.data.control_change_message.controller,
                                                message.data.control_change_message.value);
      last_non_realtime_send_time = get_absolute_time();
      break;
    case MidiMessageType::PITCH_BEND:
      MIDI::internal::_sendPitchBend_actual(message.data.pitch_bend_message.channel,
                                            message.data.pitch_bend_message.bend_value);
      last_non_realtime_send_time = get_absolute_time();
      break;
    case MidiMessageType::SYSTEM_REALTIME:
      MIDI::internal::_sendRealTime_actual(message.data.system_realtime_message.type);
      // Real-time messages do not update last_non_realtime_send_time
      break;
    case MidiMessageType::SYSTEM_EXCLUSIVE:
      MIDI::internal::_sendSysEx_actual(message.data.system_exclusive_message.length,
                                        message.data.system_exclusive_message.data_buffer.data());
      last_non_realtime_send_time = get_absolute_time();
      break;
    }
  }
}

} // namespace musin::midi
