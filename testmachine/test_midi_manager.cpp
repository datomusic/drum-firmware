#include "test_midi_manager.h"

#include "musin/hal/logger.h"
#include "musin/midi/midi_input_queue.h"
#include "musin/midi/midi_wrapper.h"

#include "etl/variant.h"
#include <cassert>

namespace testmachine {

TestMidiManager *TestMidiManager::instance_ = nullptr;

TestMidiManager::TestMidiManager(musin::Logger &logger) : logger_(logger) {
  assert(!instance_ && "Only one TestMidiManager instance is allowed.");
  instance_ = this;
}

void TestMidiManager::init() {
  logger_.info("Initializing Test MIDI Manager...");
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

void TestMidiManager::process_input() {
  MIDI::read();

  musin::midi::IncomingMidiMessage message;
  while (musin::midi::dequeue_incoming_midi_message(message)) {
    etl::visit(
        [this](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, musin::midi::NoteOnData>) {
            if (arg.velocity > 0) {
              handle_note_on(arg.channel, arg.note, arg.velocity);
            } else {
              handle_note_off(arg.channel, arg.note, arg.velocity);
            }
          } else if constexpr (std::is_same_v<T, musin::midi::NoteOffData>) {
            handle_note_off(arg.channel, arg.note, arg.velocity);
          }
        },
        message);
  }
}

void TestMidiManager::set_note_on_callback(NoteCallback callback) {
  note_on_callback_ = std::move(callback);
}

void TestMidiManager::set_note_off_callback(NoteCallback callback) {
  note_off_callback_ = std::move(callback);
}

void TestMidiManager::clear_callbacks() {
  note_on_callback_ = nullptr;
  note_off_callback_ = nullptr;
}

void TestMidiManager::note_on_callback(uint8_t channel, uint8_t note,
                                       uint8_t velocity) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::NoteOnData{channel, note, velocity});
}

void TestMidiManager::note_off_callback(uint8_t channel, uint8_t note,
                                        uint8_t velocity) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::NoteOffData{channel, note, velocity});
}

void TestMidiManager::cc_callback(uint8_t channel, uint8_t controller,
                                  uint8_t value) {
  musin::midi::enqueue_incoming_midi_message(
      musin::midi::ControlChangeData{channel, controller, value});
}

void TestMidiManager::sysex_callback(uint8_t *data, unsigned length) {
  (void)data;
  (void)length;
}

void TestMidiManager::clock_callback() {}

void TestMidiManager::start_callback() {}

void TestMidiManager::continue_callback() {}

void TestMidiManager::stop_callback() {}

void TestMidiManager::handle_note_on(uint8_t channel, uint8_t note,
                                     uint8_t velocity) {
  if (note_on_callback_) {
    note_on_callback_(channel, note, velocity);
  }
}

void TestMidiManager::handle_note_off(uint8_t channel, uint8_t note,
                                      uint8_t velocity) {
  if (note_off_callback_) {
    note_off_callback_(channel, note, velocity);
  }
}

} // namespace testmachine
