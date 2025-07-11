#include "midi_test_support.h"
#include "musin/hal/null_logger.h"
#include "musin/midi/midi_output_queue.h" // For midi_output_queue and MIDI_QUEUE_SIZE
#include <algorithm>                       // For std::equal, std::min
#include <utility>                         // For std::move

// --- Mock Time Implementation ---
// Time functions are defined as static inline in the mock pico/time.h
// and are included via test_support.h in the test files.
// We only need to provide the storage for the global mock_current_time.
absolute_time_t mock_current_time = 0;

// --- Mock Logger ---
static musin::NullLogger test_logger;

// --- Mock MIDI Call Recording Implementation ---
std::vector<MockMidiCallRecord> mock_midi_calls;

void reset_mock_midi_calls() {
  mock_midi_calls.clear();
}

// --- MockMidiCallRecord Implementation ---
MockMidiCallRecord::MockMidiCallRecord()
    : channel(0), p1(0), p2(0), p_int(0), rt_type(::midi::InvalidType), sysex_length(0) {}

MockMidiCallRecord::MockMidiCallRecord(std::string name, uint8_t ch, uint8_t param1,
                                       uint8_t param2, int paramInt, ::midi::MidiType realtimeType,
                                       std::vector<uint8_t> sx_data, unsigned sx_len)
    : function_name(std::move(name)), channel(ch), p1(param1), p2(param2), p_int(paramInt),
      rt_type(realtimeType), sysex_data(std::move(sx_data)), sysex_length(sx_len) {}

MockMidiCallRecord MockMidiCallRecord::NoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
  return {"_sendNoteOn_actual", ch, note, vel, 0, ::midi::InvalidType, {}, 0};
}
MockMidiCallRecord MockMidiCallRecord::NoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
  return {"_sendNoteOff_actual", ch, note, vel, 0, ::midi::InvalidType, {}, 0};
}
MockMidiCallRecord MockMidiCallRecord::ControlChange(uint8_t ch, uint8_t ctrl, uint8_t val) {
  return {"_sendControlChange_actual", ch, ctrl, val, 0, ::midi::InvalidType, {}, 0};
}
MockMidiCallRecord MockMidiCallRecord::PitchBend(uint8_t ch, int bend) {
  return {"_sendPitchBend_actual", ch, 0, 0, bend, ::midi::InvalidType, {}, 0};
}
MockMidiCallRecord MockMidiCallRecord::RealTime(::midi::MidiType type) {
  return {"_sendRealTime_actual", 0, 0, 0, 0, type, {}, 0};
}
MockMidiCallRecord MockMidiCallRecord::SysEx(unsigned length, const uint8_t *bytes) {
  std::vector<uint8_t> data_vec;
  if (bytes && length > 0) {
    data_vec.assign(bytes, bytes + length);
  }
  return {"_sendSysEx_actual", 0, 0, 0, 0, ::midi::InvalidType, std::move(data_vec), length};
}

bool MockMidiCallRecord::operator==(const MockMidiCallRecord &other) const {
  return function_name == other.function_name && channel == other.channel && p1 == other.p1 &&
         p2 == other.p2 && p_int == other.p_int && rt_type == other.rt_type &&
         sysex_length == other.sysex_length &&
         std::equal(sysex_data.begin(), sysex_data.end(), other.sysex_data.begin(),
                    other.sysex_data.end());
}

// --- Mock MIDI::internal Function Implementations ---
namespace MIDI::internal {
void _sendNoteOn_actual(uint8_t channel, uint8_t note, uint8_t velocity) {
  mock_midi_calls.push_back(MockMidiCallRecord::NoteOn(channel, note, velocity));
}
void _sendNoteOff_actual(uint8_t channel, uint8_t note, uint8_t velocity) {
  mock_midi_calls.push_back(MockMidiCallRecord::NoteOff(channel, note, velocity));
}
void _sendControlChange_actual(uint8_t channel, uint8_t controller, uint8_t value) {
  mock_midi_calls.push_back(MockMidiCallRecord::ControlChange(channel, controller, value));
}
void _sendPitchBend_actual(uint8_t channel, int bend) {
  mock_midi_calls.push_back(MockMidiCallRecord::PitchBend(channel, bend));
}
void _sendRealTime_actual(::midi::MidiType message) {
  mock_midi_calls.push_back(MockMidiCallRecord::RealTime(message));
}
void _sendSysEx_actual(unsigned length, const uint8_t *bytes) {
  mock_midi_calls.push_back(MockMidiCallRecord::SysEx(length, bytes));
}
} // namespace MIDI::internal

// --- Test State Management ---
// Rate limiting constant from midi_message_queue.cpp for test reference
constexpr uint32_t MIN_INTERVAL_US_NON_REALTIME_TEST = 960;

void reset_test_state() {
  reset_mock_midi_calls();

  const uint64_t significant_time_jump =
      MIN_INTERVAL_US_NON_REALTIME_TEST * musin::midi::MIDI_QUEUE_SIZE * 2;
  advance_mock_time_us(significant_time_jump);

  while (!musin::midi::midi_output_queue.empty()) {
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);
    musin::midi::process_midi_output_queue(test_logger);
  }
  reset_mock_midi_calls();

  advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);
}
