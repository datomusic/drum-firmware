#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_sender.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/sync_out.h"
#include "musin/ui/keypad_hc138.h"
#include "musin/usb/usb.h"

extern "C" {
#include "pico/stdio_usb.h"
#include "pico/time.h"
}

#include <cstdio>

#include "hardware/watchdog.h"

#include "command/command_parser.h"
#include "command/response_formatter.h"
#include "config.h"
#include "drum/drum_pizza_hardware.h"
#include "status_display.h"
#include "test_framework/test_manager.h"
#include "test_midi_manager.h"
#include "tests/midi/midi_loopback_test.h"
#include "tests/midi/midi_thru_test.h"
#include "tests/sync/sync_loopback_test.h"

#ifdef VERBOSE
static musin::PicoLogger logger(musin::LogLevel::DEBUG);
#else
static musin::NullLogger logger;
#endif

static musin::NullLogger null_logger;

// Hardware
static musin::timing::SyncIn sync_in(DATO_SUBMARINE_SYNC_IN_PIN,
                                     DATO_SUBMARINE_SYNC_DETECT_PIN);
static musin::timing::SyncOut sync_out(DATO_SUBMARINE_SYNC_OUT_PIN);

static musin::midi::MidiSender midi_sender(musin::midi::MidiSendStrategy::QUEUED,
                                           null_logger);

// Test framework
static testmachine::TestMidiManager midi_manager(logger);
static testmachine::TestManager test_manager;
static testmachine::CommandParser command_parser;
static testmachine::StatusDisplay status_display(logger);

// Test instances
static testmachine::MidiLoopbackTest midi_loopback_test(midi_manager, midi_sender);
static testmachine::MidiThruTest midi_thru_test(midi_manager, midi_sender);
static testmachine::SyncLoopbackTest sync_loopback_test(sync_out, sync_in);

// Keypad hardware
static musin::ui::Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad(
    keypad_decoder_pins, keypad_columns_pins, logger,
    drum::config::keypad::POLL_INTERVAL_MS,
    drum::config::keypad::DEBOUNCE_TIME_MS,
    drum::config::keypad::HOLD_TIME_MS,
    drum::config::keypad::TAP_TIME_MS);

namespace {

// Keypad event handler for triggering tests
class KeypadEventHandler : public etl::observer<musin::ui::KeypadEvent> {
public:
  void notification(musin::ui::KeypadEvent event) override {
    // Output JSON for all keypad events
    const char *event_type = "unknown";
    switch (event.type) {
    case musin::ui::KeypadEvent::Type::Press:
      event_type = "press";
      break;
    case musin::ui::KeypadEvent::Type::Release:
      event_type = "release";
      break;
    case musin::ui::KeypadEvent::Type::Hold:
      event_type = "hold";
      break;
    case musin::ui::KeypadEvent::Type::Tap:
      event_type = "tap";
      break;
    }

    printf("KEYPAD:{\"event\":\"%s\",\"row\":%u,\"col\":%u}\n", event_type,
           static_cast<unsigned int>(event.row),
           static_cast<unsigned int>(event.col));

    // Only respond to Press events to trigger tests
    if (event.type != musin::ui::KeypadEvent::Type::Press) {
      return;
    }

    // Check if a test is already running
    if (test_manager.is_test_running()) {
      logger.warn("Test already running, ignoring button press");
      return;
    }

    // Map keypad buttons to tests
    // Row 4, Col 3 -> SYNC_LOOPBACK
    // Row 5, Col 3 -> MIDI_LOOPBACK
    testmachine::Test *test = nullptr;

    if (event.row == 4 && event.col == 3) {
      test = &sync_loopback_test;
    } else if (event.row == 5 && event.col == 3) {
      test = &midi_loopback_test;
    }

    if (test != nullptr) {
      logger.info(test->get_name());
      test_manager.start_test(test);
      status_display.set_status(test->get_name(),
                                testmachine::DisplayStatus::RUNNING);
      status_display.update();
    }
  }
};

static KeypadEventHandler keypad_handler;

void handle_command(const testmachine::Command &cmd) {
  using testmachine::ResponseFormatter;

  if (cmd.name == "PING") {
    ResponseFormatter::send_pong();
  } else if (cmd.name == "VERSION") {
    ResponseFormatter::send_version();
  } else if (cmd.name == "LIST_TESTS") {
    ResponseFormatter::send_test_list(test_manager);
  } else if (cmd.name == "STATUS") {
    if (test_manager.is_test_running()) {
      ResponseFormatter::send_busy(test_manager.get_active_test()->get_name());
    } else {
      ResponseFormatter::send_ok("ready");
    }
  } else {
    // Check if it's a test command
    if (test_manager.is_test_running()) {
      ResponseFormatter::send_busy(test_manager.get_active_test()->get_name());
      return;
    }

    // Try to find and start a test
    testmachine::Test *test = test_manager.find_test(cmd.name);
    if (test != nullptr) {
      test_manager.start_test(test);
      status_display.set_status(test->get_name(),
                                testmachine::DisplayStatus::RUNNING);
      status_display.update();
      ResponseFormatter::send_ok("started");
    } else {
      ResponseFormatter::send_error("unknown command");
    }
  }
}

} // namespace

int main() {
  stdio_usb_init();

#ifdef VERBOSE
  musin::usb::init(true);
#else
  musin::usb::init(false);
  watchdog_enable(4000, false);
#endif

  midi_manager.init();
  status_display.init();

  // Initialize keypad
  keypad.init();
  keypad.add_observer(keypad_handler);

  // Register tests
  test_manager.register_test(&midi_loopback_test);
  test_manager.register_test(&midi_thru_test);
  test_manager.register_test(&sync_loopback_test);

  // Register test LED mappings
  status_display.register_test(midi_loopback_test.get_name(), 3, 4);
  status_display.register_test(sync_loopback_test.get_name(), 4, 4);
  status_display.register_test(midi_thru_test.get_name(), 3, 1);

  logger.info("Test machine started");

  status_display.set_status(midi_loopback_test.get_name(),
                            testmachine::DisplayStatus::IDLE);
  status_display.set_status(sync_loopback_test.get_name(),
                            testmachine::DisplayStatus::IDLE);
  status_display.set_status(midi_thru_test.get_name(),
                            testmachine::DisplayStatus::IDLE);
  status_display.update();

  while (true) {
    absolute_time_t now = get_absolute_time();

    // Scan keypad for button presses
    keypad.scan();

    // Process serial commands
    command_parser.update();
    if (command_parser.has_command()) {
      handle_command(command_parser.get_command());
    }

    // Update active test
    if (test_manager.is_test_running()) {
      test_manager.update(now);
    }

    // Check for test completion
    if (test_manager.is_test_complete()) {
      testmachine::Test *test = test_manager.get_active_test();
      testmachine::TestResult result = test_manager.get_result();
      testmachine::ResponseFormatter::send_result(test->get_name(), result);

      // Update status display based on result
      if (result.status == testmachine::TestStatus::Passed) {
        status_display.set_status(test->get_name(),
                                  testmachine::DisplayStatus::PASSED);
      } else {
        status_display.set_status(test->get_name(),
                                  testmachine::DisplayStatus::FAILED);
      }
      status_display.update();

      test_manager.clear_result();
    }

    // Update hardware
    musin::usb::background_update();
    midi_manager.process_input();
    musin::midi::process_midi_output_queue(null_logger);
    sync_in.update(now);

#ifndef VERBOSE
    watchdog_update();
#endif
  }

  return 0;
}
