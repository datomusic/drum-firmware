#ifndef TESTMACHINE_TEST_MIDI_MANAGER_H
#define TESTMACHINE_TEST_MIDI_MANAGER_H

#include <cstdint>
#include <functional>

namespace musin {
class Logger;
}

namespace testmachine {

using NoteCallback = std::function<void(uint8_t channel, uint8_t note, uint8_t velocity)>;

class TestMidiManager {
public:
  explicit TestMidiManager(musin::Logger &logger);

  void init();

  void process_input();

  void set_note_on_callback(NoteCallback callback);

  void set_note_off_callback(NoteCallback callback);

  void clear_callbacks();

private:
  musin::Logger &logger_;
  NoteCallback note_on_callback_;
  NoteCallback note_off_callback_;

  static TestMidiManager *instance_;

  static void note_on_callback(uint8_t channel, uint8_t note, uint8_t velocity);
  static void note_off_callback(uint8_t channel, uint8_t note, uint8_t velocity);
  static void cc_callback(uint8_t channel, uint8_t controller, uint8_t value);
  static void sysex_callback(uint8_t *data, unsigned length);
  static void clock_callback();
  static void start_callback();
  static void continue_callback();
  static void stop_callback();

  void handle_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
  void handle_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
};

} // namespace testmachine

#endif // TESTMACHINE_TEST_MIDI_MANAGER_H
