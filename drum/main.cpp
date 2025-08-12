#include "musin/hal/debug_utils.h"
#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
#include "musin/midi/midi_input_queue.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_sender.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/sync_out.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include "drum/configuration_manager.h"
#include "drum/drum_pizza_hardware.h"
#include "drum/midi_manager.h"
#include "drum/sysex_handler.h"
#include "musin/filesystem/filesystem.h"
#include "sample_repository.h"

extern "C" {
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
}

#ifndef VERBOSE
#include "hardware/watchdog.h"
#endif

#include <cstdio>

#include "audio_engine.h"
#include "drum/ui/pizza_display.h"
#include "events.h"
#include "message_router.h"
#include "pizza_controls.h"
#include "sequencer_controller.h"

enum class ApplicationState { SequencerMode, FileTransferMode };

#ifdef VERBOSE
static musin::PicoLogger logger;
static musin::hal::DebugUtils::LoopTimer loop_timer(10000);
#else
static musin::NullLogger logger;
#endif

// State Machine
static ApplicationState current_state = ApplicationState::SequencerMode;

struct StateMachineObserver
    : public etl::observer<drum::Events::SysExTransferStateChangeEvent> {
  void
  notification(const drum::Events::SysExTransferStateChangeEvent &event) override {
    if (event.is_active) {
      logger.debug("Entering FileTransferMode");
      current_state = ApplicationState::FileTransferMode;
    } else {
      logger.debug("Entering SequencerMode");
      current_state = ApplicationState::SequencerMode;
    }
  }
};
static StateMachineObserver state_machine_observer;

// Model
static drum::ConfigurationManager config_manager(logger);
static drum::SampleRepository sample_repository(logger);
static drum::SysExHandler sysex_handler(config_manager, logger);
static drum::AudioEngine audio_engine(sample_repository, logger);
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::MidiClockProcessor midi_clock_processor;
static musin::timing::SyncIn sync_in(DATO_SUBMARINE_SYNC_IN_PIN,
                                     DATO_SUBMARINE_SYNC_DETECT_PIN);
static musin::timing::ClockMultiplier clock_multiplier(12); // 2 PPQN to 24 PPQN
static musin::timing::TempoHandler
    tempo_handler(internal_clock, midi_clock_processor, sync_in,
                  clock_multiplier,
                  drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER,
                  musin::timing::ClockSource::INTERNAL);
static drum::SequencerController<drum::config::NUM_TRACKS,
                                 drum::config::NUM_STEPS_PER_TRACK>
    sequencer_controller(tempo_handler);

static musin::midi::MidiSender
    midi_sender(musin::midi::MidiSendStrategy::QUEUED,
                logger); // Change to DIRECT_BYPASS_QUEUE for testing bypass
static drum::MessageRouter message_router(audio_engine, sequencer_controller,
                                          midi_sender, logger);

// MIDI Manager
static drum::MidiManager midi_manager(message_router, midi_clock_processor,
                                      sysex_handler, logger);

// View
static drum::PizzaDisplay pizza_display(sequencer_controller, tempo_handler,
                                        logger);

// Controller
static drum::PizzaControls pizza_controls(pizza_display, tempo_handler,
                                          sequencer_controller, message_router,
                                          logger);

static musin::timing::SyncOut sync_out(DATO_SUBMARINE_SYNC_OUT_PIN,
                                       internal_clock);

int main() {
  stdio_usb_init();

#ifdef VERBOSE
  musin::usb::init(true); // Wait for serial connection in debug builds
#else
  musin::usb::init(false); // Do not wait in release builds
  watchdog_enable(4000, false);
#endif

  if (!musin::filesystem::init(false)) {
    // Filesystem is not critical for basic operation if no samples are present,
    // but we should log the failure.
    logger.warn("Failed to initialize filesystem.");
  } else {
    musin::filesystem::list_files("/"); // List files in the root directory

    config_manager.load();
  }

  midi_manager.init();

  audio_engine.init();
  message_router.set_output_mode(drum::OutputMode::BOTH);

  pizza_display.init();
  pizza_controls.init();
  pizza_display.start_boot_animation();

  // --- Initialize Clocking System ---
  // TempoHandler's constructor calls set_clock_source, which handles initial
  // observation.
  sync_in.add_observer(clock_multiplier);
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(
      pizza_display); // PizzaDisplay needs tempo events for pulsing

  // SequencerController notifies MessageRouter, which queues the events
  // internally.
  sequencer_controller.add_observer(message_router);

  // Register observers for SysEx state changes
  sysex_handler.add_observer(message_router);
  sysex_handler.add_observer(pizza_display);
  sysex_handler.add_observer(sequencer_controller);
  sysex_handler.add_observer(state_machine_observer);

  // Register observers for events from MessageRouter
  message_router.add_note_event_observer(pizza_display);
  message_router.add_parameter_change_event_observer(pizza_display);
  message_router.add_note_event_observer(audio_engine);

  sync_out.enable();

  while (true) {
    absolute_time_t now = get_absolute_time();

    switch (current_state) {
    case ApplicationState::SequencerMode: {
      sysex_handler.update(now);
      pizza_controls.update(now);
      sync_in.update(now);
      clock_multiplier.update(now);
      sequencer_controller
          .update(); // Checks if a step is due and queues NoteEvents
      message_router
          .update(); // Drains NoteEvent queue, sending to observers and MIDI
      audio_engine.process();
      pizza_display.update(now);
      musin::usb::background_update();
      midi_manager.process_input();
      tempo_handler.update();
      musin::midi::process_midi_output_queue(
          logger); // Pass logger to queue processing
      sleep_us(10);
      break;
    }
    case ApplicationState::FileTransferMode: {
      // In file transfer mode, we only service the bare essentials
      // to keep the transfer fast.
      sysex_handler.update(now);
      pizza_display.update(
          now); // Keep display alive for progress updates
      musin::usb::background_update();
      midi_manager.process_input();
      musin::midi::process_midi_output_queue(
          logger); // For sending ACKs
      break;
    }
    }

#ifndef VERBOSE
    // Watchdog update for Release builds
    watchdog_update();
#else
    loop_timer.record_iteration_end();
#endif
  }

  return 0;
}
