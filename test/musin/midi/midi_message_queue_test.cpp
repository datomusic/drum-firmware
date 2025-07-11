#include "musin/midi/midi_output_queue.h"
#include "test_support.h" // Assumed to bring in Catch2
#include "midi_test_support.h"
#include "musin/hal/null_logger.h"

// Rate limiting constant from midi_message_queue.cpp for test reference
// This should match the value in the implementation file.
// We define it here to ensure tests are aware of the value without exposing it in the header.
constexpr uint32_t MIN_INTERVAL_US_NON_REALTIME_TEST = 960;

// --- Mock Logger ---
static musin::NullLogger test_logger;

// Helper function to get mock time, useful for debugging or more complex assertions
// Not strictly necessary for these tests but good practice for mock time modules.
absolute_time_t get_mock_time_us() {
  return mock_current_time;
}


TEST_CASE("MidiMessageQueue Tests", "[midi_queue]") {
  using namespace musin::midi;

  SECTION("Basic Enqueue and Process") {
    reset_test_state();
    OutgoingMidiMessage msg(1, 60, 100, true); // Note On, Ch 1, Note 60, Vel 100
    REQUIRE(enqueue_midi_message(msg, test_logger));
    REQUIRE_FALSE(midi_output_queue.empty());

    process_midi_output_queue(test_logger);

    REQUIRE(mock_midi_calls.size() == 1);
    REQUIRE(mock_midi_calls[0] ==
            MockMidiCallRecord::NoteOn(1, 60, 100));
    REQUIRE(midi_output_queue.empty());
  }

  SECTION("Queue Full Behavior") {
    reset_test_state();

    // Fill queue completely with valid MIDI notes (0-127)
    const uint8_t first_note = 60;
    const uint8_t last_note = std::min<uint8_t>(127, first_note + MIDI_QUEUE_SIZE - 1);
    for (uint8_t note = first_note; note <= last_note; ++note) {
      OutgoingMidiMessage msg(1, note, 100, true);
      REQUIRE(enqueue_midi_message(msg, test_logger));
    }
    REQUIRE(midi_output_queue.full());

    // Try to add one more message (should fail)
    const uint8_t extra_note = (last_note < 127) ? last_note + 1 : 0;
    OutgoingMidiMessage extra_msg(1, extra_note, 100, true);
    REQUIRE_FALSE(enqueue_midi_message(extra_msg, test_logger));

    // Process all queued messages
    while (!midi_output_queue.empty()) {
      advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);
      process_midi_output_queue(test_logger);
    }

    // Verify all initial messages were processed in order
    REQUIRE(mock_midi_calls.size() == (last_note - first_note + 1));
    for (size_t i = 0; i < mock_midi_calls.size(); ++i) {
      REQUIRE(mock_midi_calls[i] ==
              MockMidiCallRecord::NoteOn(1, first_note + i, 100));
    }

    // Now queue should be empty - add our extra message
    reset_mock_midi_calls();
    REQUIRE(enqueue_midi_message(extra_msg, test_logger));
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);
    process_midi_output_queue(test_logger);

    // Verify extra message was processed
    REQUIRE(mock_midi_calls.size() == 1);
    REQUIRE(mock_midi_calls[0] ==
            MockMidiCallRecord::NoteOn(1, extra_note, 100));
  }

  SECTION("Processing an Empty Queue") {
    reset_test_state();
    REQUIRE(midi_output_queue.empty());
    process_midi_output_queue(test_logger);
    REQUIRE(mock_midi_calls.empty());
  }

  SECTION("FIFO Order") {
    reset_test_state();

    // Time is now set by reset_test_state() to allow the first message to send.
    INFO("Initial time: " << get_mock_time_us());

    OutgoingMidiMessage note_on_msg(1, 60, 100, true);
    OutgoingMidiMessage cc_msg(1, 7, 127); // CC, Ch 1, Ctrl 7, Val 127

    REQUIRE(enqueue_midi_message(note_on_msg, test_logger));
    REQUIRE(enqueue_midi_message(cc_msg, test_logger));

    INFO("Queue size after enqueue: " << midi_output_queue.size());

    // First process - should send Note On
    process_midi_output_queue(test_logger);
    INFO("After first process - mock calls: " << mock_midi_calls.size());
    INFO("Current time: " << get_mock_time_us());

    REQUIRE(mock_midi_calls.size() == 1);
    if (!mock_midi_calls.empty()) {
      INFO("First call: " << mock_midi_calls[0].function_name
                          << " ch=" << mock_midi_calls[0].channel
                          << " p1=" << mock_midi_calls[0].p1
                          << " p2=" << mock_midi_calls[0].p2);
    }
    REQUIRE(mock_midi_calls[0] ==
            MockMidiCallRecord::NoteOn(1, 60, 100));

    // Advance time and process CC
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);
    INFO("Advanced time to: " << get_mock_time_us());

    process_midi_output_queue(test_logger);
    INFO("After second process - mock calls: " << mock_midi_calls.size());

    REQUIRE(mock_midi_calls.size() == 2);
    if (mock_midi_calls.size() > 1) {
      INFO("Second call: " << mock_midi_calls[1].function_name
                           << " ch=" << mock_midi_calls[1].channel
                           << " p1=" << mock_midi_calls[1].p1
                           << " p2=" << mock_midi_calls[1].p2);
    }
    REQUIRE(mock_midi_calls[1] ==
            MockMidiCallRecord::ControlChange(1, 7, 127));
    REQUIRE(midi_output_queue.empty());
  }

  SECTION("Rate Limiting for Non-Real-Time Messages") {
    reset_test_state();
    OutgoingMidiMessage cc_msg1(1, 10, 50);
    OutgoingMidiMessage cc_msg2(1, 11, 60);

    REQUIRE(enqueue_midi_message(cc_msg1, test_logger));
    process_midi_output_queue(test_logger); // Send cc_msg1
    REQUIRE(mock_midi_calls.size() == 1);
    REQUIRE(mock_midi_calls[0] ==
            MockMidiCallRecord::ControlChange(1, 10, 50));

    REQUIRE(enqueue_midi_message(cc_msg2, test_logger));
    process_midi_output_queue(test_logger); // Attempt to send cc_msg2, should be deferred
    REQUIRE(mock_midi_calls.size() == 1); // Still 1, not sent yet
    REQUIRE_FALSE(midi_output_queue.empty());

    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST / 2);
    process_midi_output_queue(test_logger); // Still deferred
    REQUIRE(mock_midi_calls.size() == 1);

    // Advance time just enough to meet the interval
    advance_mock_time_us((MIN_INTERVAL_US_NON_REALTIME_TEST / 2) +
                         (MIN_INTERVAL_US_NON_REALTIME_TEST % 2));
    process_midi_output_queue(test_logger); // Should send now
    REQUIRE(mock_midi_calls.size() == 2);
    REQUIRE(mock_midi_calls[1] ==
            MockMidiCallRecord::ControlChange(1, 11, 60));
    REQUIRE(midi_output_queue.empty());
  }

  SECTION("Real-Time Messages Bypass Rate Limiting") {
    reset_test_state();
    OutgoingMidiMessage cc_msg(1, 10, 50);
    OutgoingMidiMessage clock_msg(::midi::Clock);

    REQUIRE(enqueue_midi_message(cc_msg, test_logger));
    process_midi_output_queue(test_logger); // Send cc_msg
    REQUIRE(mock_midi_calls.size() == 1);
    REQUIRE(mock_midi_calls[0] ==
            MockMidiCallRecord::ControlChange(1, 10, 50));
    absolute_time_t time_after_cc = get_mock_time_us(); // Use the mock time getter

    REQUIRE(enqueue_midi_message(clock_msg, test_logger));
    process_midi_output_queue(test_logger); // Send clock_msg immediately
    REQUIRE(mock_midi_calls.size() == 2);
    REQUIRE(mock_midi_calls[1] ==
            MockMidiCallRecord::RealTime(::midi::Clock));

    // Check that last_non_realtime_send_time was not updated by the RT message
    OutgoingMidiMessage cc_msg2(1, 11, 60);
    REQUIRE(enqueue_midi_message(cc_msg2, test_logger));
    // Since RT message did not update last_non_realtime_send_time,
    // cc_msg2 should be deferred based on time_after_cc.
    process_midi_output_queue(test_logger);
    REQUIRE(mock_midi_calls.size() == 2); // cc_msg2 deferred

    set_mock_time_us(time_after_cc + MIN_INTERVAL_US_NON_REALTIME_TEST);
    process_midi_output_queue(test_logger); // Now cc_msg2 should send
    REQUIRE(mock_midi_calls.size() == 3);
    REQUIRE(mock_midi_calls[2] ==
            MockMidiCallRecord::ControlChange(1, 11, 60));
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

    REQUIRE(enqueue_midi_message(msg_note_on, test_logger));
    process_midi_output_queue(test_logger);
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_note_off, test_logger));
    process_midi_output_queue(test_logger);
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_cc, test_logger));
    process_midi_output_queue(test_logger);
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_pitch_bend, test_logger));
    process_midi_output_queue(test_logger);
    advance_mock_time_us(MIN_INTERVAL_US_NON_REALTIME_TEST);

    REQUIRE(enqueue_midi_message(msg_rt, test_logger)); // Real-time
    process_midi_output_queue(test_logger);
    // No time advance needed after RT for next non-RT, as RT doesn't affect its timer

    REQUIRE(enqueue_midi_message(msg_sysex, test_logger));
    // SysEx is non-RT. If the previous non-RT was recent, this might be deferred.
    // The previous non-RT was pitch_bend, then RT, so the timer for non-RT is from pitch_bend.
    // We advanced time after pitch_bend, so this should send.
    process_midi_output_queue(test_logger);

    REQUIRE(mock_midi_calls.size() == 6);
    REQUIRE(mock_midi_calls[0] ==
            MockMidiCallRecord::NoteOn(ch, note, vel));
    REQUIRE(mock_midi_calls[1] ==
            MockMidiCallRecord::NoteOff(ch, note, vel));
    REQUIRE(mock_midi_calls[2] ==
            MockMidiCallRecord::ControlChange(ch, ctrl, val));
    REQUIRE(mock_midi_calls[3] ==
            MockMidiCallRecord::PitchBend(ch, bend));
    REQUIRE(mock_midi_calls[4] ==
            MockMidiCallRecord::RealTime(rt_type));
    REQUIRE(mock_midi_calls[5] ==
            MockMidiCallRecord::SysEx(sysex_len, sysex_payload));
  }

  SECTION("System Exclusive Message Handling") {
    reset_test_state();
    SECTION("Normal SysEx") {
      reset_test_state();
      uint8_t payload[] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};
      unsigned len = sizeof(payload);
      OutgoingMidiMessage msg(payload, len);
      REQUIRE(enqueue_midi_message(msg, test_logger));
      process_midi_output_queue(test_logger);
      REQUIRE(mock_midi_calls.size() == 1);
      REQUIRE(mock_midi_calls[0] ==
              MockMidiCallRecord::SysEx(len, payload));
      REQUIRE(mock_midi_calls[0].sysex_length == len);
      REQUIRE(std::equal(mock_midi_calls[0].sysex_data.begin(),
                         mock_midi_calls[0].sysex_data.end(), payload,
                         payload + len));
    }

    SECTION("Empty SysEx (nullptr payload, zero length in constructor)") {
      reset_test_state();
      OutgoingMidiMessage msg(nullptr, 0);
      REQUIRE(enqueue_midi_message(msg, test_logger));
      process_midi_output_queue(test_logger);
      REQUIRE(mock_midi_calls.size() == 1);
      // Constructor sets length to 0 if payload is nullptr
      REQUIRE(mock_midi_calls[0] ==
              MockMidiCallRecord::SysEx(0, nullptr));
      REQUIRE(mock_midi_calls[0].sysex_length == 0);
      REQUIRE(mock_midi_calls[0].sysex_data.empty());
    }

    SECTION("SysEx Truncation (longer than MIDI::SysExMaxSize)") {
      reset_test_state();
      std::vector<uint8_t> long_payload_vec(MIDI::SysExMaxSize + 10);
      for (size_t i = 0; i < long_payload_vec.size(); ++i) {
        long_payload_vec[i] = static_cast<uint8_t>(i);
      }

      OutgoingMidiMessage msg(long_payload_vec.data(),
                              static_cast<unsigned>(long_payload_vec.size()));
      REQUIRE(enqueue_midi_message(msg, test_logger));
      process_midi_output_queue(test_logger);

      REQUIRE(mock_midi_calls.size() == 1);
      // The constructor of OutgoingMidiMessage truncates.
      REQUIRE(mock_midi_calls[0].function_name == "_sendSysEx_actual");
      REQUIRE(mock_midi_calls[0].sysex_length == MIDI::SysExMaxSize);
      REQUIRE(mock_midi_calls[0].sysex_data.size() == MIDI::SysExMaxSize);
      // Compare the truncated data
      REQUIRE(std::equal(mock_midi_calls[0].sysex_data.begin(),
                         mock_midi_calls[0].sysex_data.end(),
                         long_payload_vec.begin(), long_payload_vec.begin() + MIDI::SysExMaxSize));
    }

    SECTION("SysEx with zero length but non-null pointer in constructor") {
      reset_test_state();
      uint8_t dummy_payload[] = {1, 2, 3}; // Content doesn't matter as length is 0
      OutgoingMidiMessage msg(dummy_payload, 0);
      REQUIRE(enqueue_midi_message(msg, test_logger));
      process_midi_output_queue(test_logger);
      REQUIRE(mock_midi_calls.size() == 1);
      REQUIRE(mock_midi_calls[0] ==
              MockMidiCallRecord::SysEx(0, dummy_payload));
      REQUIRE(mock_midi_calls[0].sysex_length == 0);
      REQUIRE(mock_midi_calls[0].sysex_data.empty());
    }
  }
}
