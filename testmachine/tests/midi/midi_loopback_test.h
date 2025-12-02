#ifndef TESTMACHINE_TESTS_MIDI_LOOPBACK_TEST_H
#define TESTMACHINE_TESTS_MIDI_LOOPBACK_TEST_H

#include "testmachine/test_framework/test_interface.h"
#include "testmachine/test_midi_manager.h"

#include "musin/midi/midi_sender.h"

#include "etl/array.h"

namespace testmachine {

class MidiLoopbackTest : public Test {
public:
  static constexpr uint8_t NUM_TEST_NOTES = 8;
  static constexpr uint8_t START_NOTE = 60;
  static constexpr uint8_t TEST_VELOCITY = 64;
  static constexpr uint8_t TEST_CHANNEL = 1;
  static constexpr uint32_t SEND_INTERVAL_MS = 50;
  static constexpr uint32_t DEFAULT_TIMEOUT_MS = 5000;

  MidiLoopbackTest(TestMidiManager &midi_manager,
                   musin::midi::MidiSender &midi_sender);

  const char *get_name() const override { return "MIDI_LOOPBACK"; }

  void start(absolute_time_t now) override;
  void update(absolute_time_t now) override;
  bool is_complete() const override;
  TestResult get_result() const override;
  void reset() override;

  void set_timeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

private:
  void on_note_received(uint8_t channel, uint8_t note, uint8_t velocity);

  struct NoteRecord {
    uint8_t note = 0;
    bool sent = false;
    bool received = false;
  };

  TestMidiManager &midi_manager_;
  musin::midi::MidiSender &midi_sender_;

  etl::array<NoteRecord, NUM_TEST_NOTES> notes_;
  uint8_t send_index_ = 0;
  absolute_time_t next_send_time_;
  absolute_time_t timeout_time_;
  uint32_t timeout_ms_ = DEFAULT_TIMEOUT_MS;

  bool complete_ = false;
  TestResult result_;
};

} // namespace testmachine

#endif // TESTMACHINE_TESTS_MIDI_LOOPBACK_TEST_H
