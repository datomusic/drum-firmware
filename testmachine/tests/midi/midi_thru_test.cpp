#include "midi_thru_test.h"

#include <cstdio>

namespace testmachine {

MidiThruTest::MidiThruTest(TestMidiManager &midi_manager,
                           musin::midi::MidiSender &midi_sender)
    : midi_manager_(midi_manager), midi_sender_(midi_sender) {}

void MidiThruTest::start(absolute_time_t now) {
  complete_ = false;
  messages_forwarded_ = 0;
  result_ = {TestStatus::Running, ""};
  end_time_ = delayed_by_ms(now, duration_ms_);

  midi_manager_.set_note_on_callback(
      [this](uint8_t ch, uint8_t note, uint8_t vel) {
        on_note_on(ch, note, vel);
      });

  midi_manager_.set_note_off_callback(
      [this](uint8_t ch, uint8_t note, uint8_t vel) {
        on_note_off(ch, note, vel);
      });
}

void MidiThruTest::update(absolute_time_t now) {
  if (complete_) {
    return;
  }

  if (time_reached(end_time_)) {
    complete_ = true;
    char msg[64];
    snprintf(msg, sizeof(msg), "forwarded %lu messages", messages_forwarded_);
    result_ = TestResult::passed(msg);
    midi_manager_.clear_callbacks();
  }
}

bool MidiThruTest::is_complete() const { return complete_; }

TestResult MidiThruTest::get_result() const { return result_; }

void MidiThruTest::reset() {
  complete_ = false;
  messages_forwarded_ = 0;
  result_ = {TestStatus::NotStarted, ""};
  midi_manager_.clear_callbacks();
}

void MidiThruTest::on_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
  midi_sender_.sendNoteOn(channel, note, velocity);
  ++messages_forwarded_;
}

void MidiThruTest::on_note_off(uint8_t channel, uint8_t note,
                               uint8_t velocity) {
  midi_sender_.sendNoteOff(channel, note, velocity);
  ++messages_forwarded_;
}

} // namespace testmachine
