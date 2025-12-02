#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_sender.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/sync_out.h"
#include "musin/usb/usb.h"

extern "C" {
#include "pico/stdio_usb.h"
#include "pico/time.h"
}

#include "hardware/watchdog.h"

#include "command/command_parser.h"
#include "command/response_formatter.h"
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

namespace {

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
      status_display.set_status(testmachine::DisplayStatus::RUNNING);
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

  // Register tests
  test_manager.register_test(&midi_loopback_test);
  test_manager.register_test(&midi_thru_test);
  test_manager.register_test(&sync_loopback_test);

  logger.info("Test machine started");

  status_display.set_status(testmachine::DisplayStatus::IDLE);
  status_display.update();

  while (true) {
    absolute_time_t now = get_absolute_time();

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
        status_display.set_status(testmachine::DisplayStatus::PASSED);
      } else {
        status_display.set_status(testmachine::DisplayStatus::FAILED);
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
