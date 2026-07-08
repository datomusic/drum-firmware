#include "musin/hal/debug_utils.h"
#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/midi/midi_sender.h"
#include "musin/timing/clock_router.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_out.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/speed_adapter.h"
#include "musin/timing/sync_in.h"
#include "musin/timing/sync_out.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include "drum/configuration_manager.h"
#include "drum/firmware_update_buyer.h"
#include "drum/midi_manager.h"
#include "drum/settings_manager.h"
#include "drum/sysex_handler.h"
#include "musin/filesystem/filesystem.h"
#include "sample_repository.h"
#include "sample_slot_manager.h"

extern "C" {
#include "pico/stdio_usb.h"
#include "pico/time.h"
}

#include "hardware/watchdog.h"
#include "pico/rand.h"

#include <cstdlib>

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

// Model
static drum::ConfigurationManager config_manager(logger);
static drum::settings::Settings settings;
static drum::SettingsManager settings_manager(settings, logger);
static drum::SampleRepository sample_repository(logger);
static musin::filesystem::Filesystem filesystem(logger);
static drum::SysExHandler sysex_handler(config_manager, settings_manager,
                                        logger, filesystem);
static drum::SampleSlotManager sample_slot_manager(logger);
static drum::AudioEngine audio_engine(sample_repository, sample_slot_manager,
                                      logger);
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::MidiClockProcessor midi_clock_processor;
static musin::timing::SyncIn sync_in(DATO_SUBMARINE_SYNC_IN_PIN,
                                     DATO_SUBMARINE_SYNC_DETECT_PIN);
static musin::timing::ClockRouter
    clock_router(internal_clock, midi_clock_processor, sync_in,
                 musin::timing::ClockSource::INTERNAL);
static musin::timing::SpeedAdapter
    speed_adapter(musin::timing::SpeedModifier::NORMAL_SPEED);
static musin::timing::SyncOut sync_out(DATO_SUBMARINE_SYNC_OUT_PIN);
static musin::timing::TempoHandler
    tempo_handler(clock_router, speed_adapter,
                  drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER,
                  musin::timing::ClockSource::INTERNAL);
static musin::timing::MidiClockOut
    midi_clock_out(tempo_handler,
                   drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER);
static drum::SequencerController<drum::config::NUM_TRACKS,
                                 drum::config::NUM_STEPS_PER_TRACK>
    sequencer_controller(tempo_handler, logger);

static musin::midi::MidiSender midi_sender(
    musin::midi::MidiSendStrategy::QUEUED,
    null_logger); // Change to DIRECT_BYPASS_QUEUE for testing bypass
static drum::MessageRouter message_router(audio_engine, sequencer_controller,
                                          midi_sender, settings, logger);

// MIDI Manager
static drum::MidiManager midi_manager(message_router, midi_clock_processor,
                                      sysex_handler, settings, logger);

// View
static drum::PizzaDisplay pizza_display(sequencer_controller, tempo_handler,
                                        logger);

static drum::SystemStateMachine system_state_machine(logger);

// Controller
static drum::PizzaControls pizza_controls(pizza_display, tempo_handler,
                                          sequencer_controller, message_router,
                                          system_state_machine, logger);

int main() {
  stdio_usb_init();

  // Seed rand() from the hardware RNG; seeding from the boot clock in static
  // constructors produced the same sequence every power-up.
  srand(get_rand_32());

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
    settings_manager.init();

    // Initialize persistence subsystem now that filesystem is ready
    sequencer_controller.init_persistence();

    // Preload each voice's active sample into RAM before audio starts.
    for (uint8_t track = 0; track < drum::config::NUM_TRACKS; ++track) {
      const uint8_t note =
          sequencer_controller.get_active_note_for_track(track);
      auto path = sample_repository.get_path(note);
      if (path.has_value() &&
          sample_slot_manager.request_load(track, note, *path)) {
        sample_slot_manager.commit_staging();
      }
    }
  }

  tempo_handler.set_tempo_control_range(
      drum::config::analog_controls::MIN_BPM_ADJUST,
      drum::config::analog_controls::MAX_BPM_ADJUST);

  midi_manager.init();

  audio_engine.init();
  audio_engine.set_line_in_routing(
      static_cast<drum::config::audio::LineInRouting>(
          settings.get(drum::settings::Id::LineInRouting)));
  message_router.set_output_mode(drum::OutputMode::BOTH);

  pizza_display.init();
  pizza_controls.init();

  // --- Initialize Clocking System ---
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(
      pizza_display); // PizzaDisplay needs tempo events for pulsing
  tempo_handler.add_observer(
      pizza_controls); // PizzaControls needs tempo events for sample cycling
  // SyncOut observes ClockRouter for raw 24 PPQN ticks

  // SequencerController notifies MessageRouter, which queues the events
  // internally.
  sequencer_controller.add_observer(message_router);

  // Connect SysExHandler to SequencerController for sequencer state transfer
  sysex_handler.set_sequencer_state_access(&sequencer_controller);

  // A completed SDS sample transfer must drop any RAM copy of the slot so
  // the rewritten file is re-read from flash (issue #572).
  sysex_handler.set_sample_slot_manager(&sample_slot_manager);

  // Register observers for SysEx state changes
  sysex_handler.add_observer(message_router);
  sysex_handler.add_observer(sequencer_controller);
  sysex_handler.add_observer(system_state_machine);
  sysex_handler.add_observer(pizza_display);

  // Register observers for events from MessageRouter
  message_router.add_note_event_observer(pizza_display);
  message_router.add_parameter_change_event_observer(pizza_display);
  message_router.add_note_event_observer(audio_engine);

  sync_out.enable();

  clock_router.set_sync_out(&sync_out);
  clock_router.add_observer(sync_out);
  clock_router.add_observer(midi_clock_out);
  clock_router.add_observer(speed_adapter);

  // SystemStateMachine starts in Boot state.

  // Commits a try-before-you-buy image once the main loop has proven healthy.
  // Constructed here (not statically) so the bootrom query runs after SDK
  // runtime init.
  static drum::FirmwareUpdateBuyer firmware_update_buyer(logger);

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
      firmware_update_buyer.update(now);
      sysex_handler.set_firmware_update_allowed(
          !firmware_update_buyer.is_trial_boot());
      sysex_handler.update(now);
      pizza_controls.update(now);
      sync_in.update(now);
      sequencer_controller
          .update(); // Checks if a step is due and queues NoteEvents
      message_router
          .update(); // Drains NoteEvent queue, sending to observers and MIDI
      audio_engine.set_line_in_routing(
          static_cast<drum::config::audio::LineInRouting>(
              settings.get(drum::settings::Id::LineInRouting)));
      audio_engine.process();
      pizza_display.update(now);
      midi_manager.process_input();
      musin::midi::process_midi_output_queue(null_logger);
      internal_clock.update(now);
      clock_router.update_auto_source_switching();

      // Second drain: process_midi_output_queue sends at most one message
      // per call, so draining twice per loop halves MIDI output latency
      // (see #527).
      musin::midi::process_midi_output_queue(null_logger);
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
