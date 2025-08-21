#include "drum/midi_manager.h"

#include "drum/message_router.h"
#include "drum/sysex_handler.h"
#include "musin/hal/logger.h"
#include "musin/midi/midi_input_queue.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/timing/midi_clock_processor.h"

#include "etl/span.h"
#include "etl/variant.h"
#include <cassert>

namespace drum {

// Initialize the static instance pointer.
MidiManager *MidiManager::instance_ = nullptr;

MidiManager::MidiManager(
    MessageRouter &message_router,
    musin::timing::MidiClockProcessor &midi_clock_processor,
    SysExHandler &sysex_handler, musin::Logger &logger)
    : message_router_(message_router),
      midi_clock_processor_(midi_clock_processor),
      sysex_handler_(sysex_handler), logger_(logger) {
  assert(!instance_ && "Only one MidiManager instance is allowed.");
  instance_ = this;
}

void MidiManager::init() {
  logger_.info("Initializing MIDI Manager...");
  MIDI::init(MIDI::Callbacks{
      .note_on = note_on_callback,
      .note_off = note_off_callback,
      .clock = clock_callback,
      .start = start_callback,
      .cont = continue_callback,
      .stop = stop_callback,
      .cc = cc_callback,
      .pitch_bend = nullptr,
      .sysex = sysex_callback,
  });
}

void MidiManager::process_input() {
  MIDI::read();

  musin::midi::IncomingMidiMessage message;
  while (musin::midi::dequeue_incoming_midi_message(message)) {
    // Gate all non-SysEx messages during a file transfer to prevent conflicts.
    if (sysex_handler_.is_busy() &&
        !etl::holds_alternative<sysex::Chunk>(message)) {
      continue;
    }

    etl::visit(
        [this](auto &&arg) {
          using T = typename std::decay<decltype(arg)>::type;
          if constexpr (std::is_same_v<T, musin::midi::NoteOnData>) {
            if (arg.velocity > 0) {
              handle_note_on(arg.channel, arg.note, arg.velocity);
            } else {
              // Note On with velocity 0 is a Note Off
              if constexpr (!drum::config::IGNORE_MIDI_NOTE_OFF) {
                handle_note_off(arg.channel, arg.note, arg.velocity);
              }
            }
          } else if constexpr (std::is_same_v<T, musin::midi::NoteOffData>) {
            if constexpr (!drum::config::IGNORE_MIDI_NOTE_OFF) {
              handle_note_off(arg.channel, arg.note, arg.velocity);
            }
          } else if constexpr (std::is_same_v<T,
                                              musin::midi::ControlChangeData>) {
            handle_control_change(arg.channel, arg.controller, arg.value);
          } else if constexpr (std::is_same_v<
                                   T, musin::midi::SystemRealtimeData>) {
            handle_realtime(arg.type);
          } else if constexpr (std::is_same_v<T, sysex::Chunk>) {
            handle_sysex(arg);
          }
        },
        message);
  }
}

// --- C-style Callbacks ---

void MidiManager::note_on_callback(uint8_t channel, uint8_t note,
                                   uint8_t velocity) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::NoteOnData{channel, note, velocity});
}

void MidiManager::note_off_callback(uint8_t channel, uint8_t note,
                                    uint8_t velocity) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::NoteOffData{channel, note, velocity});
}

void MidiManager::cc_callback(uint8_t channel, uint8_t controller,
                              uint8_t value) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::ControlChangeData{channel, controller, value});
}

void MidiManager::sysex_callback(uint8_t *data, unsigned length) {
  // The underlying MIDI library passes the full SysEx message, including the
  // start (0xF0) and end (0xF7) bytes. We slice these off.
  if (length < 2) {
    return;
  }
  musin::midi::enqueue_incoming_midi_message(
      sysex::Chunk(data + 1, length - 2));
}

void MidiManager::clock_callback() {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::SystemRealtimeData{::midi::Clock});
}

void MidiManager::start_callback() {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::SystemRealtimeData{::midi::Start});
}

void MidiManager::continue_callback() {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::SystemRealtimeData{::midi::Continue});
}

void MidiManager::stop_callback() {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::SystemRealtimeData{::midi::Stop});
}

// --- Message Handlers ---

void MidiManager::handle_note_on(uint8_t channel, uint8_t note,
                                 uint8_t velocity) {
  if (channel != drum::config::MIDI_IN_CHANNEL) {
    return; // Ignore messages not on our input channel
  }
  message_router_.handle_incoming_note_on(note, velocity);
}

void MidiManager::handle_note_off(uint8_t channel, uint8_t note,
                                  uint8_t velocity) {
  if (channel != drum::config::MIDI_IN_CHANNEL) {
    return; // Ignore messages not on our input channel
  }
  message_router_.handle_incoming_note_off(note, velocity);
}

void MidiManager::handle_control_change(uint8_t channel, uint8_t controller,
                                        uint8_t value) {
  if (channel != drum::config::MIDI_IN_CHANNEL) {
    return; // Ignore messages not on our input channel
  }
  message_router_.handle_incoming_midi_cc(controller, value);
}

void MidiManager::handle_sysex(const sysex::Chunk &chunk) {
  sysex_handler_.handle_sysex_message(chunk);
}

void MidiManager::handle_realtime(uint16_t type) {
  // This could be a switch, but for now, only clock is handled.
  if (type == ::midi::Clock) {
    midi_clock_processor_.on_midi_clock_tick_received();
  }
  // TODO: Handle Start, Stop, Continue if needed by the clock processor
}

} // namespace drum
