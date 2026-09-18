#include "midi_loopback_test.h"

#include <cstdio>

namespace testmachine {

MidiLoopbackTest::MidiLoopbackTest(TestMidiManager &midi_manager,
                                   musin::midi::MidiSender &midi_sender)
    : midi_manager_(midi_manager), midi_sender_(midi_sender) {}

void MidiLoopbackTest::start(absolute_time_t now) {
  complete_ = false;
  send_index_ = 0;
  result_ = {TestStatus::Running, ""};

  for (uint8_t i = 0; i < NUM_TEST_NOTES; ++i) {
    notes_[i] = {TEST_NOTE_SEQUENCE[i], false, false};
  }

  next_send_time_ = now;
  timeout_time_ = delayed_by_ms(now, timeout_ms_);

  midi_manager_.set_note_on_callback(
      [this](uint8_t ch, uint8_t note, uint8_t vel) {
        on_note_received(ch, note, vel);
      });
}

void MidiLoopbackTest::update(absolute_time_t now) {
  if (complete_) {
    return;
  }

  if (time_reached(timeout_time_)) {
    complete_ = true;
    uint8_t received = 0;
    for (const auto &note : notes_) {
      if (note.received) {
        ++received;
      }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "timeout: received %u/%u", received,
             NUM_TEST_NOTES);
    result_ = TestResult::timeout(msg);
    midi_manager_.clear_callbacks();
    return;
  }

  if (send_index_ < NUM_TEST_NOTES && time_reached(next_send_time_)) {
    midi_sender_.sendNoteOn(TEST_CHANNEL, notes_[send_index_].note,
                            TEST_VELOCITY);
    notes_[send_index_].sent = true;
    ++send_index_;
    next_send_time_ = delayed_by_ms(now, SEND_INTERVAL_MS);
  }

  bool all_received = true;
  for (const auto &note : notes_) {
    if (!note.received) {
      all_received = false;
      break;
    }
  }

  if (all_received) {
    complete_ = true;
    result_ = TestResult::passed("all notes received");
    midi_manager_.clear_callbacks();
  }
}

bool MidiLoopbackTest::is_complete() const { return complete_; }

TestResult MidiLoopbackTest::get_result() const { return result_; }

void MidiLoopbackTest::reset() {
  complete_ = false;
  send_index_ = 0;
  result_ = {TestStatus::NotStarted, ""};
  midi_manager_.clear_callbacks();
}

void MidiLoopbackTest::on_note_received(uint8_t channel, uint8_t note,
                                        uint8_t velocity) {
  (void)velocity;

  if (channel != TEST_CHANNEL) {
    return;
  }

  for (auto &rec : notes_) {
    if (rec.note == note && !rec.received) {
      rec.received = true;
      break;
    }
  }
}

} // namespace testmachine
