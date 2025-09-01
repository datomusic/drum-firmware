#include "musin/midi/midi_output_queue.h"
#include "etl/deque.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::internal actual send functions
#include "pico/sync.h"
#include "pico/time.h" // For RP2040 specific timing (get_absolute_time, absolute_time_diff_us, is_nil_time)
#include <cstdio>      // For printf
#include <optional>

namespace musin::midi {

// Define the global queue instance and its spinlock
etl::deque<OutgoingMidiMessage, MIDI_QUEUE_SIZE> midi_output_queue;
static spin_lock_t *midi_queue_lock =
    spin_lock_init(spin_lock_claim_unused(true));

bool enqueue_midi_message(const OutgoingMidiMessage &message,
                          musin::Logger &logger) {
  uint32_t irq_status = spin_lock_blocking(midi_queue_lock);

  // Coalesce Control Change messages
  if (message.type == MidiMessageType::CONTROL_CHANGE) {
    for (auto &queued_message : midi_output_queue) {
      if (queued_message.type == MidiMessageType::CONTROL_CHANGE &&
          queued_message.data.control_change_message.channel ==
              message.data.control_change_message.channel &&
          queued_message.data.control_change_message.controller ==
              message.data.control_change_message.controller) {
        // Found an existing CC for the same channel/controller, update it
        queued_message.data.control_change_message.value =
            message.data.control_change_message.value;
        spin_unlock(midi_queue_lock, irq_status);
        return true; // Don't enqueue a new one
      }
    }
  }

  // If no message was coalesced, enqueue this one if there's space
  bool success = false;
  if (!midi_output_queue.full()) {
    midi_output_queue.push_back(message);
    success = true;
  } else {
    // Log queue full error
    logger.debug("MIDI queue full - message dropped");
  }

  spin_unlock(midi_queue_lock, irq_status);
  return success;
}

// Rate limiting for non-real-time messages
constexpr uint32_t MIN_INTERVAL_US_NON_REALTIME =
    960; // 3125 bytes per second at 3 bytes each
static absolute_time_t last_non_realtime_send_time = nil_time;

void process_midi_output_queue(musin::Logger &logger) {
  std::optional<OutgoingMidiMessage> message_to_send;
  bool rate_limited = false;

  { // Critical section for accessing the queue
    uint32_t irq_status = spin_lock_blocking(midi_queue_lock);
    if (!midi_output_queue.empty()) {
      OutgoingMidiMessage &message_in_queue = midi_output_queue.front();
      if (message_in_queue.type == MidiMessageType::SYSTEM_REALTIME ||
          is_nil_time(last_non_realtime_send_time) ||
          absolute_time_diff_us(last_non_realtime_send_time,
                                get_absolute_time()) >=
              MIN_INTERVAL_US_NON_REALTIME) {
        message_to_send = message_in_queue;
        midi_output_queue.pop_front();
      } else {
        rate_limited = true;
      }
    }
    spin_unlock(midi_queue_lock, irq_status);
  }

  if (message_to_send) {
    OutgoingMidiMessage message = *message_to_send;
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
      MIDI::internal::_sendControlChange_actual(
          message.data.control_change_message.channel,
          message.data.control_change_message.controller,
          message.data.control_change_message.value);
      last_non_realtime_send_time = get_absolute_time();
      break;
    case MidiMessageType::PITCH_BEND:
      MIDI::internal::_sendPitchBend_actual(
          message.data.pitch_bend_message.channel,
          message.data.pitch_bend_message.bend_value);
      last_non_realtime_send_time = get_absolute_time();
      break;
    case MidiMessageType::SYSTEM_REALTIME:
      MIDI::internal::_sendRealTime_actual(
          message.data.system_realtime_message.type);
      // Real-time messages do not update last_non_realtime_send_time
      break;
    case MidiMessageType::SYSTEM_EXCLUSIVE:
      MIDI::internal::_sendSysEx_actual(
          message.data.system_exclusive_message.length,
          message.data.system_exclusive_message.data_buffer.data());
      last_non_realtime_send_time = get_absolute_time();
      break;
    }
  } else if (rate_limited) {
    // This logging is not strictly necessary but can be useful for debugging
    // rate limiting. It's kept outside the lock to avoid holding it during I/O.
    // int64_t time_since_last =
    //     is_nil_time(last_non_realtime_send_time)
    //         ? -1
    //         : absolute_time_diff_us(last_non_realtime_send_time,
    //         get_absolute_time());
    // logger.log(LogLevel::DEBUG, "Deferred MIDI message",
    // static_cast<int32_t>(time_since_last));
  }
}

} // namespace musin::midi
