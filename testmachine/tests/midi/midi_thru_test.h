#ifndef TESTMACHINE_TESTS_MIDI_THRU_TEST_H
#define TESTMACHINE_TESTS_MIDI_THRU_TEST_H

#include "testmachine/test_framework/test_interface.h"
#include "testmachine/test_midi_manager.h"

#include "musin/midi/midi_sender.h"

namespace testmachine {

class MidiThruTest : public Test {
public:
  static constexpr uint32_t DEFAULT_DURATION_MS = 5000;

  MidiThruTest(TestMidiManager &midi_manager,
               musin::midi::MidiSender &midi_sender);

  const char *get_name() const override { return "MIDI_THRU"; }

  void start(absolute_time_t now) override;
  void update(absolute_time_t now) override;
  bool is_complete() const override;
  TestResult get_result() const override;
  void reset() override;

  void set_duration(uint32_t duration_ms) { duration_ms_ = duration_ms; }

private:
  void on_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
  void on_note_off(uint8_t channel, uint8_t note, uint8_t velocity);

  TestMidiManager &midi_manager_;
  musin::midi::MidiSender &midi_sender_;

  absolute_time_t end_time_;
  uint32_t duration_ms_ = DEFAULT_DURATION_MS;
  uint32_t messages_forwarded_ = 0;

  bool complete_ = false;
  TestResult result_;
};

} // namespace testmachine

#endif // TESTMACHINE_TESTS_MIDI_THRU_TEST_H
