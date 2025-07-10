#ifndef MIDI_TEST_SUPPORT_H
#define MIDI_TEST_SUPPORT_H

#include "musin/midi/midi_wrapper.h" // For ::midi::MidiType
#include "pico/time.h"               // For absolute_time_t
#include <string>
#include <vector>

// Forward declare the mock implementations in the MIDI::internal namespace
namespace MIDI::internal {
void _sendNoteOn_actual(uint8_t channel, uint8_t note, uint8_t velocity);
void _sendNoteOff_actual(uint8_t channel, uint8_t note, uint8_t velocity);
void _sendControlChange_actual(uint8_t channel, uint8_t controller, uint8_t value);
void _sendPitchBend_actual(uint8_t channel, int bend);
void _sendRealTime_actual(::midi::MidiType message);
void _sendSysEx_actual(unsigned length, const uint8_t *bytes);
} // namespace MIDI::internal

// Mock time management is handled by the mock pico/time.h included in tests.
// We just declare the extern variable here for visibility.
extern absolute_time_t mock_current_time;

// Mock MIDI call recording structure and utilities
struct MockMidiCallRecord {
  std::string function_name;
  uint8_t channel;
  uint8_t p1; // Note or Controller
  uint8_t p2; // Velocity or Value
  int p_int;  // Bend value
  ::midi::MidiType rt_type;
  std::vector<uint8_t> sysex_data;
  unsigned sysex_length;

  // Constructors
  MockMidiCallRecord();
  MockMidiCallRecord(std::string name, uint8_t ch, uint8_t param1, uint8_t param2, int paramInt,
                     ::midi::MidiType realtimeType, std::vector<uint8_t> sx_data, unsigned sx_len);

  // Factory methods for convenience
  static MockMidiCallRecord NoteOn(uint8_t ch, uint8_t note, uint8_t vel);
  static MockMidiCallRecord NoteOff(uint8_t ch, uint8_t note, uint8_t vel);
  static MockMidiCallRecord ControlChange(uint8_t ch, uint8_t ctrl, uint8_t val);
  static MockMidiCallRecord PitchBend(uint8_t ch, int bend);
  static MockMidiCallRecord RealTime(::midi::MidiType type);
  static MockMidiCallRecord SysEx(unsigned length, const uint8_t *bytes);

  bool operator==(const MockMidiCallRecord &other) const;
};

extern std::vector<MockMidiCallRecord> mock_midi_calls;

void reset_mock_midi_calls();
void reset_test_state();

#endif // MIDI_TEST_SUPPORT_H
