#include "musin/midi/midi_message_queue.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::internal actual send functions
#include "pico/time.h"      // For RP2040 specific timing (get_absolute_time, absolute_time_diff_us, is_nil_time)
#include <cstdio>           // For printf

namespace musin::midi {

// Define the global queue instance
etl::queue_spsc_atomic<OutgoingMidiMessage, MIDI_QUEUE_SIZE, etl::memory_model::MEMORY_MODEL_SMALL> midi_output_queue;

bool enqueue_midi_message(const OutgoingMidiMessage& message) {
    // For SPSC queue, push is ISR safe if this is the single producer context.
    // If multiple contexts (ISRs, main loop) call this, a MPMC queue or explicit locking around push() would be safer.
    // For now, we proceed with SPSC, assuming careful usage or future refinement if needed.
    // The `etl::queue_spsc_atomic` is designed for one producer and one consumer.
    // If `enqueue_midi_message` can be called from multiple ISRs or from an ISR and the main thread
    // concurrently, then the `push` operation itself might need external protection if those
    // calls could interleave at an instruction level that affects the queue's internal atomic operations.
    // However, typically, the atomicity provided by the queue handles this for single-word operations.
    if (!midi_output_queue.full()) {
        midi_output_queue.push(message);
        return true;
    }
    // Optional: Handle queue full error (e.g., log, drop oldest, etc.)
    printf("MIDI QUEUE FULL!\n");
    return false; // Dropping new message if queue is full
}

// Rate limiting for non-real-time messages
constexpr uint32_t MIN_INTERVAL_US_NON_REALTIME = 2000; // 2ms (2000us) between CC/Note/SysEx
static absolute_time_t last_non_realtime_send_time = nil_time;

void process_midi_output_queue() {
    if (midi_output_queue.empty()) {
        return;
    }

    // Peek at the message to decide if we can send it now (due to rate limiting)
    OutgoingMidiMessage& message_to_process = midi_output_queue.front();

    bool can_send = false;
    if (message_to_process.type == MidiMessageType::SYSTEM_REALTIME) {
        can_send = true; // Real-time messages are sent immediately
    } else {
        // Check rate limit for non-real-time messages
        if (is_nil_time(last_non_realtime_send_time) || 
            absolute_time_diff_us(last_non_realtime_send_time, get_absolute_time()) >= MIN_INTERVAL_US_NON_REALTIME) {
            can_send = true;
        }
    }

    if (!can_send && message_to_process.type == MidiMessageType::NOTE_ON) {
        printf("MIDI Q: Note send deferred by rate limit.\n");
    }

    if (can_send) {
        // Make a copy and pop before sending, in case sending itself triggers another enqueue.
        OutgoingMidiMessage message = message_to_process; // Copy the message
        midi_output_queue.pop();                         // Remove from queue

        switch (message.type) {
            case MidiMessageType::NOTE_ON:
                printf("MIDI Q: Processing NOTE_ON ch=%d n=%d v=%d\n", message.data.note_message.channel, message.data.note_message.note, message.data.note_message.velocity); // DEBUG
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
    // If a non-real-time message was deferred due to rate limiting,
    // it remains at the front of the queue and will be re-evaluated
    // on the next call to process_midi_output_queue().
}

} // namespace musin::midi
