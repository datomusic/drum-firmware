#include "test_support.h" // Assumed to bring in Catch2
#include "musin/midi/midi_message_queue.h"
#include "musin/midi/midi_wrapper.h" // For MIDI::internal declarations and ::midi::MidiType
#include "pico/time.h"               // For absolute_time_t definition from mock

#include <vector>
#include <string>
#include <cstdint>
#include <algorithm> // For std::copy, std::equal
#include <vector>    // Already included but good for clarity

// Definition of the global mock time variable declared in mock pico/time.h
absolute_time_t mock_current_time = 0;

// Rate limiting constant from midi_message_queue.cpp for test reference
// This should match the value in musin/midi/midi_message_queue.cpp
constexpr uint32_t MIN_INTERVAL_US_NON_REALTIME_TEST = 960;

namespace MIDI::internal {

struct MockMidiCallRecord {
  std::string function_name;
  uint8_t channel;
  uint8_t p1; // Note or Controller
  uint8_t p2; // Velocity or Value
  int p_int;  // Bend value
  ::midi::MidiType rt_type;
  std::vector<uint8_t> sysex_data;
  unsigned sysex_length;

  // Default constructor
  MockMidiCallRecord()
      : channel(0), p1(0), p2(0), p_int(0), rt_type(::midi::InvalidType), sysex_length(0) {
  }

  // Parameterized constructor
  MockMidiCallRecord(std::string name, uint8_t ch, uint8_t param1, uint8_t param2, int paramInt,
                     ::midi::MidiType realtimeType, std::vector<uint8_t> sx_data, // Pass by value
                     unsigned sx_len)
      : function_name(std::move(name)), channel(ch), p1(param1), p2(param2), p_int(paramInt),
        rt_type(realtimeType), sysex_data(std::move(sx_data)), sysex_length(sx_len) {
  }

  static MockMidiCallRecord NoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    return {"_sendNoteOn_actual", ch, note, vel, 0, ::midi::InvalidType, {}, 0};
  }
  static MockMidiCallRecord NoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
    return {"_sendNoteOff_actual", ch, note, vel, 0, ::midi::InvalidType, {}, 0};
  }
  static MockMidiCallRecord ControlChange(uint8_t ch, uint8_t ctrl, uint8_t val) {
    return {"_sendControlChange_actual", ch, ctrl, val, 0, ::midi::InvalidType, {}, 0};
  }
  static MockMidiCallRecord PitchBend(uint8_t ch, int bend) {
    return {"_sendPitchBend_actual", ch, 0, 0, bend, ::midi::InvalidType, {}, 0};
  }
  static MockMidiCallRecord RealTime(::midi::MidiType type) {
    return {"_sendRealTime_actual", 0, 0, 0, 0, type, {}, 0};
  }
  static MockMidiCallRecord SysEx(unsigned length, const uint8_t *bytes) {
    std::vector<uint8_t> data_vec;
    if (bytes && length > 0) {
      data_vec.assign(bytes, bytes + length);
    }
    return {"_sendSysEx_actual", 0, 0, 0, 0, ::midi::InvalidType, std::move(data_vec), length};
  }

  // Equality operator for easy comparison in tests
  bool operator==(const MockMidiCallRecord &other) const {
    return function_name == other.function_name && channel == other.channel && p1 == other.p1 &&
           p2 == other.p2 && p_int == other.p_int && rt_type == other.rt_type &&
           sysex_length == other.sysex_length &&
           // Use std::equal for vector comparison
           std::equal(sysex_data.begin(), sysex_data.end(), other.sysex_data.begin(),
                      other.sysex_data.end());
  }
};

std::vector<MockMidiCallRecord> mock_midi_calls;

void reset_mock_midi_calls() {
  mock_midi_calls.clear();
}

// Mock implementations for MIDI::internal functions
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

// Helper to reset all mocks and queue state for a clean test environment
void reset_test_state() {
  MIDI::internal::reset_mock_midi_calls();
  set_mock_time_us(0);
  // Clear the actual queue by processing any remaining items
  // This loop ensures that if processing one item allows another (due to time advance),
  // it also gets processed.
  while (!musin::midi::midi_output_queue.empty()) {
    // Advance time significantly to ensure any rate-limited items are processed
    // This ensures that the queue is truly empty before the next test section.
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST * 2);
    musin::midi::process_midi_output_queue();
  }
  MIDI::internal::reset_mock_midi_calls(); // Reset calls again after clearing queue
  set_mock_time_us(0);                     // Reset time again for a clean start
}

TEST_CASE("MidiMessageQueue Tests", "[midi_queue]") {
  using namespace musin::midi;

  SECTION("Basic Enqueue and Process") {
    reset_test_state();
    OutgoingMidiMessage msg(1, 60, 100, true); // Note On, Ch 1, Note 60, Vel 100
    REQUIRE(enqueue_midi_message(msg));
    REQUIRE_FALSE(midi_output_queue.empty());

    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::NoteOn(1, 60, 100));
    REQUIRE(midi_output_queue.empty());
  }

  SECTION("Queue Full Behavior") {
    reset_test_state();
    for (size_t i = 0; i < MIDI_QUEUE_SIZE; ++i) {
      OutgoingMidiMessage msg(1, static_cast<uint8_t>(60 + i), 100, true);
      REQUIRE(enqueue_midi_message(msg));
    }
    REQUIRE(midi_output_queue.full());

    OutgoingMidiMessage extra_msg(1, 120, 100, true);
    REQUIRE_FALSE(enqueue_midi_message(extra_msg)); // Should fail

    // Process one message to make space
    process_midi_output_queue();
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1); // One message processed
    REQUIRE_FALSE(midi_output_queue.full());

    REQUIRE(enqueue_midi_message(extra_msg)); // Should succeed now
    // Process the newly enqueued message (extra_msg)
    // Need to advance time if it's non-realtime and the previous was also non-realtime
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);
    process_midi_output_queue();
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 2);
    REQUIRE(MIDI::internal::mock_midi_calls[1] ==
            MIDI::internal::MockMidiCallRecord::NoteOn(1, 120, 100));
  }

  SECTION("Processing an Empty Queue") {
    reset_test_state();
    REQUIRE(midi_output_queue.empty());
    process_midi_output_queue();
    REQUIRE(MIDI::internal::mock_midi_calls.empty());
  }

  SECTION("FIFO Order") {
    reset_test_state();
    OutgoingMidiMessage note_on_msg(1, 60, 100, true);
    OutgoingMidiMessage cc_msg(1, 7, 127); // CC, Ch 1, Ctrl 7, Val 127

    REQUIRE(enqueue_midi_message(note_on_msg));
    REQUIRE(enqueue_midi_message(cc_msg));

    process_midi_output_queue(); // Process Note On
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::NoteOn(1, 60, 100));

    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST); // Allow CC to be sent
    process_midi_output_queue();                             // Process CC
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 2);
    REQUIRE(MIDI::internal::mock_midi_calls[1] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(1, 7, 127));
    REQUIRE(midi_output_queue.empty());
  }

  SECTION("Rate Limiting for Non-Real-Time Messages") {
    reset_test_state();
    OutgoingMidiMessage cc_msg1(1, 10, 50);
    OutgoingMidiMessage cc_msg2(1, 11, 60);

    REQUIRE(enqueue_midi_message(cc_msg1));
    process_midi_output_queue(); // Send cc_msg1
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(1, 10, 50));

    REQUIRE(enqueue_midi_message(cc_msg2));
    process_midi_output_queue(); // Attempt to send cc_msg2, should be deferred
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1); // Still 1, not sent yet
    REQUIRE_FALSE(midi_output_queue.empty());

    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST / 2);
    process_midi_output_queue(); // Still deferred
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);

    // Advance time just enough to meet the interval
    advance_mock_time_us((MIN_INTERVAL_US_NON_REALTIME_TEST / 2) +
                         (MIN_INTERVAL_US_NON_REALTIME_TEST % 2));
    process_midi_output_queue(); // Should send now
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 2);
    REQUIRE(MIDI::internal::mock_midi_calls[1] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(1, 11, 60));
    REQUIRE(midi_output_queue.empty());
  }

  SECTION("Real-Time Messages Bypass Rate Limiting") {
    reset_test_state();
    OutgoingMidiMessage cc_msg(1, 10, 50);
    OutgoingMidiMessage clock_msg(::midi::Clock);

    REQUIRE(enqueue_midi_message(cc_msg));
    process_midi_output_queue(); // Send cc_msg
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(1, 10, 50));
    absolute_time_t time_after_cc = get_mock_time_us(); // Use the mock time getter

    REQUIRE(enqueue_midi_message(clock_msg));
    process_midi_output_queue(); // Send clock_msg immediately
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 2);
    REQUIRE(MIDI::internal::mock_midi_calls[1] ==
            MIDI::internal::MockMidiCallRecord::RealTime(::midi::Clock));

    // Check that last_non_realtime_send_time was not updated by the RT message
    OutgoingMidiMessage cc_msg2(1, 11, 60);
    REQUIRE(enqueue_midi_message(cc_msg2));
    // Since RT message did not update last_non_realtime_send_time,
    // cc_msg2 should be deferred based on time_after_cc.
    process_midi_output_queue();
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 2); // cc_msg2 deferred

    set_mock_time_us(time_after_cc + MIN_INTERVAL_US_NON_REALTIME_TEST);
    process_midi_output_queue(); // Now cc_msg2 should send
    REQUIRE(MIDI::internal::mock_midi_calls.size() == 3);
    REQUIRE(MIDI::internal::mock_midi_calls[2] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(1, 11, 60));
  }

  SECTION("Test All Message Types") {
    reset_test_state();
    uint8_t ch = 1;
    uint8_t note = 60;
    uint8_t vel = 100;
    uint8_t ctrl = 20;
    uint8_t val = 80;
    int bend = 1024;
    ::midi::MidiType rt_type = ::midi::Start;
    uint8_t sysex_payload[] = {0xF0, 0x7E, 0x00, 0x09, 0x01, 0xF7};
    unsigned sysex_len = sizeof(sysex_payload);

    OutgoingMidiMessage msg_note_on(ch, note, vel, true);
    OutgoingMidiMessage msg_note_off(ch, note, vel, false);
    OutgoingMidiMessage msg_cc(ch, ctrl, val);
    OutgoingMidiMessage msg_pitch_bend(ch, bend);
    OutgoingMidiMessage msg_rt(rt_type);
    OutgoingMidiMessage msg_sysex(sysex_payload, sysex_len);

    REQUIRE(enqueue_midi_message(msg_note_on));
    process_midi_output_queue();
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_note_off));
    process_midi_output_queue();
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_cc));
    process_midi_output_queue();
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_pitch_bend));
    process_midi_output_queue();
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_rt)); // Real-time
    process_midi_output_queue();
    // No time advance needed after RT for next non-RT, as RT doesn't affect its timer

    REQUIRE(enqueue_midi_message(msg_sysex));
    // SysEx is non-RT. If the previous non-RT was recent, this might be deferred.
    // The previous non-RT was pitch_bend, then RT, so the timer for non-RT is from pitch_bend.
    // We advanced time after pitch_bend, so this should send.
    process_midi_output_queue();

    REQUIRE(MIDI::internal::mock_midi_calls.size() == 6);
    REQUIRE(MIDI::internal::mock_midi_calls[0] ==
            MIDI::internal::MockMidiCallRecord::NoteOn(ch, note, vel));
    REQUIRE(MIDI::internal::mock_midi_calls[1] ==
            MIDI::internal::MockMidiCallRecord::NoteOff(ch, note, vel));
    REQUIRE(MIDI::internal::mock_midi_calls[2] ==
            MIDI::internal::MockMidiCallRecord::ControlChange(ch, ctrl, val));
    REQUIRE(MIDI::internal::mock_midi_calls[3] ==
            MIDI::internal::MockMidiCallRecord::PitchBend(ch, bend));
    REQUIRE(MIDI::internal::mock_midi_calls[4] ==
            MIDI::internal::MockMidiCallRecord::RealTime(rt_type));
    REQUIRE(MIDI::internal::mock_midi_calls[5] ==
            MIDI::internal::MockMidiCallRecord::SysEx(sysex_len, sysex_payload));
  }

  SECTION("System Exclusive Message Handling") {
    reset_test_state();
    SECTION("Normal SysEx") {
      reset_test_state();
      uint8_t payload[] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};
      unsigned len = sizeof(payload);
      OutgoingMidiMessage msg(payload, len);
      REQUIRE(enqueue_midi_message(msg));
      process_midi_output_queue();
      REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
      REQUIRE(MIDI::internal::mock_midi_calls[0] ==
              MIDI::internal::MockMidiCallRecord::SysEx(len, payload));
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_length == len);
      REQUIRE(std::equal(MIDI::internal::mock_midi_calls[0].sysex_data.begin(),
                         MIDI::internal::mock_midi_calls[0].sysex_data.end(), payload,
                         payload + len));
    }

    SECTION("Empty SysEx (nullptr payload, zero length in constructor)") {
      reset_test_state();
      OutgoingMidiMessage msg(nullptr, 0);
      REQUIRE(enqueue_midi_message(msg));
      process_midi_output_queue();
      REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
      // Constructor sets length to 0 if payload is nullptr
      REQUIRE(MIDI::internal::mock_midi_calls[0] ==
              MIDI::internal::MockMidiCallRecord::SysEx(0, nullptr));
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_length == 0);
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_data.empty());
    }

    SECTION("SysEx Truncation (longer than MIDI::SysExMaxSize)") {
      reset_test_state();
      std::vector<uint8_t> long_payload_vec(MIDI::SysExMaxSize + 10);
      for (size_t i = 0; i < long_payload_vec.size(); ++i) {
        long_payload_vec[i] = static_cast<uint8_t>(i);
      }

      OutgoingMidiMessage msg(long_payload_vec.data(),
                              static_cast<unsigned>(long_payload_vec.size()));
      REQUIRE(enqueue_midi_message(msg));
      process_midi_output_queue();

      REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
      // The constructor of OutgoingMidiMessage truncates.
      REQUIRE(MIDI::internal::mock_midi_calls[0].function_name == "_sendSysEx_actual");
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_length == MIDI::SysExMaxSize);
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_data.size() == MIDI::SysExMaxSize);
      // Compare the truncated data
      REQUIRE(std::equal(MIDI::internal::mock_midi_calls[0].sysex_data.begin(),
                         MIDI::internal::mock_midi_calls[0].sysex_data.end(),
                         long_payload_vec.begin()));
    }

    SECTION("SysEx with zero length but non-null pointer in constructor") {
      reset_test_state();
      uint8_t dummy_payload[] = {1, 2, 3}; // Content doesn't matter as length is 0
      OutgoingMidiMessage msg(dummy_payload, 0);
      REQUIRE(enqueue_midi_message(msg));
      process_midi_output_queue();
      REQUIRE(MIDI::internal::mock_midi_calls.size() == 1);
      REQUIRE(MIDI::internal::mock_midi_calls[0] ==
              MIDI::internal::MockMidiCallRecord::SysEx(0, dummy_payload));
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_length == 0);
      REQUIRE(MIDI::internal::mock_midi_calls[0].sysex_data.empty());
    }
  }
}

// Helper function to get mock time, useful for debugging or more complex assertions
// Not strictly necessary for these tests but good practice for mock time modules.
absolute_time_t get_mock_time_us() {
  return mock_current_time;
}
