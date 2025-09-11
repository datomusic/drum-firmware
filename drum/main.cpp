#include "musin/hal/debug_utils.h"
#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_sender.h"
#include "musin/timing/clock_multiplier.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/sync_out.h"
#include "musin/timing/midi_clock_out.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include "drum/configuration_manager.h"
#include "drum/midi_manager.h"
#include "drum/sysex_handler.h"
#include "musin/filesystem/filesystem.h"
#include "sample_repository.h"

extern "C" {
#include "pico/stdio_usb.h"
#include "pico/time.h"
}

#include "hardware/watchdog.h"

#include "audio_engine.h"
#include "drum/drum_pizza_hardware.h"
#include "drum/ui/pizza_display.h"
#include "events.h"
#include "message_router.h"
#include "pizza_controls.h"
#include "sequencer_controller.h"
#include "system_state_machine.h"

#ifdef VERBOSE
static musin::PicoLogger logger(musin::LogLevel::DEBUG);
static musin::hal::DebugUtils::LoopTimer loop_timer(10000);
#else
static musin::NullLogger logger;
#endif

static musin::NullLogger null_logger;

// System State Machine (will be initialized after pizza_display)

// Model
static drum::ConfigurationManager config_manager(logger);
static drum::SampleRepository sample_repository(logger);
static musin::filesystem::Filesystem filesystem(logger);
static drum::SysExHandler sysex_handler(config_manager, logger, filesystem);
static drum::AudioEngine audio_engine(sample_repository, logger);
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::MidiClockProcessor midi_clock_processor;
static musin::timing::SyncIn sync_in(DATO_SUBMARINE_SYNC_IN_PIN,
                                     DATO_SUBMARINE_SYNC_DETECT_PIN);
// External SyncIn is 4 PPQN; convert to 24 PPQN via x6
constexpr uint8_t SYNC_TO_MIDI_CLOCK_MULTIPLIER = 6;
static musin::timing::ClockMultiplier
    clock_multiplier(SYNC_TO_MIDI_CLOCK_MULTIPLIER); // 4 PPQN to 24 PPQN
static_assert(SYNC_TO_MIDI_CLOCK_MULTIPLIER > 0,
              "Clock multiplication factor cannot be zero");
static musin::timing::ClockRouter clock_router(
    internal_clock, midi_clock_processor, clock_multiplier,
    musin::timing::ClockSource::INTERNAL);
static musin::timing::TempoHandler
    tempo_handler(internal_clock, midi_clock_processor, sync_in, clock_router,
                  drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER,
                  musin::timing::ClockSource::INTERNAL);
static musin::timing::MidiClockOut midi_clock_out(
    tempo_handler, drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER);
static drum::SequencerController<drum::config::NUM_TRACKS,
                                 drum::config::NUM_STEPS_PER_TRACK>
    sequencer_controller(tempo_handler, logger);

static musin::midi::MidiSender midi_sender(
    musin::midi::MidiSendStrategy::QUEUED,
    null_logger); // Change to DIRECT_BYPASS_QUEUE for testing bypass
static drum::MessageRouter message_router(audio_engine, sequencer_controller,
                                          midi_sender, logger);

// MIDI Manager
static drum::MidiManager midi_manager(message_router, midi_clock_processor,
                                      sysex_handler, logger);

// View
static drum::PizzaDisplay pizza_display(sequencer_controller, tempo_handler,
                                        logger);

// System State Machine (simplified without wrapper)
static drum::SystemStateMachine system_state_machine(logger);

// Controller
static drum::PizzaControls pizza_controls(pizza_display, tempo_handler,
                                          sequencer_controller, message_router,
                                          system_state_machine, logger);

static musin::timing::SyncOut sync_out(DATO_SUBMARINE_SYNC_OUT_PIN);

int main() {
  stdio_usb_init();

#ifdef VERBOSE
  musin::usb::init(true); // Wait for serial connection in debug builds
#else
  musin::usb::init(false); // Do not wait in release builds
  watchdog_enable(4000, false);
#endif

  if (!filesystem.init()) {
    // Filesystem is not critical for basic operation if no samples are present,
    // but we should log the failure.
    logger.warn("Failed to initialize filesystem.");
  } else {
    filesystem.list_files("/"); // List files in the root directory

    config_manager.load();

    // Initialize persistence subsystem now that filesystem is ready
    sequencer_controller.init_persistence();
  }

  midi_manager.init();

  audio_engine.init();
  message_router.set_output_mode(drum::OutputMode::BOTH);

  pizza_display.init();
  pizza_controls.init();

  // Boot animation is now handled by BootState
  // No circular references needed with simplified approach

  // --- Initialize Clocking System ---
  // TempoHandler's constructor calls set_clock_source, which handles initial
  // observation.
  sync_in.add_observer(clock_multiplier);
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(
      pizza_display); // PizzaDisplay needs tempo events for pulsing
  tempo_handler.add_observer(
      pizza_controls); // PizzaControls needs tempo events for sample cycling
  // SyncOut observes ClockRouter for raw 24 PPQN ticks

  // SequencerController notifies MessageRouter, which queues the events
  // internally.
  sequencer_controller.add_observer(message_router);

  // Register observers for SysEx state changes
  sysex_handler.add_observer(message_router);
  sysex_handler.add_observer(sequencer_controller);
  sysex_handler.add_observer(system_state_machine);

  // Register observers for events from MessageRouter
  message_router.add_note_event_observer(pizza_display);
  message_router.add_parameter_change_event_observer(pizza_display);
  message_router.add_note_event_observer(audio_engine);

  sync_out.enable();

  clock_router.add_observer(sync_out);
  clock_router.add_observer(midi_clock_out);

  // SystemStateMachine automatically starts in Boot state
  // No initialization_complete() call needed

  // Track state transitions for display management
  drum::SystemStateId previous_state = drum::SystemStateId::Boot;
  pizza_display.start_boot_animation(); // Initial boot animation

  while (true) {
    absolute_time_t now = get_absolute_time();

    system_state_machine.update(now);

    // Handle state transitions for display
    drum::SystemStateId current_state =
        system_state_machine.get_current_state();
    if (current_state != previous_state) {
      switch (current_state) {
      case drum::SystemStateId::Boot:
        pizza_display.start_boot_animation();
        break;
      case drum::SystemStateId::Sequencer:
        pizza_display.switch_to_sequencer_mode();
        break;
      case drum::SystemStateId::FileTransfer:
        pizza_display.switch_to_file_transfer_mode();
        break;
      case drum::SystemStateId::FallingAsleep:
        // Force save sequencer state before sleep
        if (sequencer_controller.is_persistence_initialized()) {
          if (sequencer_controller.save_state_to_flash()) {
            logger.info("State saved successfully before sleep");
          } else {
            logger.warn("Failed to save state before sleep");
          }
        }
        audio_engine.mute();
        audio_engine.deinit();
        pizza_display.start_sleep_mode();
        break;
      case drum::SystemStateId::Sleep:
        pizza_display.deinit();
        break;
      }
      previous_state = current_state;
    }

    switch (system_state_machine.get_current_state()) {
    case drum::SystemStateId::Boot: {
      // During boot, only run essential systems and display
      musin::usb::background_update();
      pizza_display.update(now);
      midi_manager.process_input();
      break;
    }
    case drum::SystemStateId::Sequencer: {
      // Full sequencer operation - existing SequencerMode logic
      musin::usb::background_update();
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
      midi_manager.process_input();
      internal_clock.update(now);
      tempo_handler.update();

      // ClockRouter handles raw clock routing; SyncOut remains attached
      musin::midi::process_midi_output_queue(
          null_logger); // Pass logger to queue processing
      sleep_us(10);
      break;
    }
    case drum::SystemStateId::FileTransfer: {
      // File transfer mode - only service bare essentials for performance
      musin::usb::background_update();
      sysex_handler.update(now);
      pizza_display.update(now); // Keep display alive for progress updates
      midi_manager.process_input();
      musin::midi::process_midi_output_queue(logger); // For sending ACKs
      break;
    }
    case drum::SystemStateId::FallingAsleep: {
      // Falling asleep mode - minimal systems during fadeout
      pizza_display.update(now);
      midi_manager.process_input();
      musin::midi::process_midi_output_queue(logger);
      sleep_us(10);
      break;
    }
    case drum::SystemStateId::Sleep: {
      // Sleep mode - minimal systems, hardware wake handled by SleepState
      // Note: Display should be off, wake detection handled by
      // SleepState::update()
      break;
    }
    }

    // Respect SyncOut behavior when stopped as clock sender (internal clock)
    bool allow_sync_out = true;
    if (tempo_handler.get_clock_source() ==
        musin::timing::ClockSource::INTERNAL) {
      if (tempo_handler.get_playback_state() ==
              musin::timing::PlaybackState::STOPPED &&
          !drum::config::SEND_SYNC_CLOCK_WHEN_STOPPED_AS_MASTER) {
        allow_sync_out = false;
      }
    }
    if (allow_sync_out) {
      sync_out.enable();
    } else {
      sync_out.disable();
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
