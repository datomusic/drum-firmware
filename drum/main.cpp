#include "musin/hal/debug_utils.h"
#include "musin/hal/null_logger.h"
#include "musin/hal/pico_logger.h"
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

// --- Model ---
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

// --- MIDI Manager ---
static drum::MidiManager midi_manager(message_router, midi_clock_processor,
                                      sysex_handler, logger);

// --- View ---
static drum::PizzaDisplay pizza_display(sequencer_controller, tempo_handler,
                                        logger);

// --- Controller and State Machine ---
// PizzaControls is created first, but its dependency on the state machine
// is injected later to break the circular dependency.
static drum::PizzaControls pizza_controls(pizza_display, tempo_handler,
                                          sequencer_controller, message_router,
                                          logger);

// The SystemStateMachine is the central orchestrator.
static drum::SystemStateMachine
    system_state_machine(logger, sysex_handler, pizza_controls, sync_in,
                         clock_multiplier, sequencer_controller, message_router,
                         audio_engine, pizza_display, midi_manager,
                         internal_clock, tempo_handler);

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
    logger.warn("Failed to initialize filesystem.");
  } else {
    musin::filesystem::list_files("/");
    config_manager.load();
  }

  midi_manager.init();
  audio_engine.init();
  message_router.set_output_mode(drum::OutputMode::BOTH);
  pizza_display.init();
  pizza_controls.init();

  // Complete the dependency loop between controls and state machine
  pizza_controls.set_system_state_machine(&system_state_machine);

  // --- Initialize Clocking System ---
  sync_in.add_observer(clock_multiplier);
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(pizza_display);
  tempo_handler.add_observer(pizza_controls);

  // --- Initialize Observers ---
  sequencer_controller.add_observer(message_router);
  sysex_handler.add_observer(message_router);
  sysex_handler.add_observer(sequencer_controller);
  sysex_handler.add_observer(system_state_machine);
  message_router.add_note_event_observer(pizza_display);
  message_router.add_parameter_change_event_observer(pizza_display);
  message_router.add_note_event_observer(audio_engine);

  sync_out.enable();

  // The SystemStateMachine constructor now handles the initial state entry,
  // including the boot animation.

  while (true) {
    absolute_time_t now = get_absolute_time();

    // The state machine now handles all state-specific logic.
    // Tell, Don't Ask.
    system_state_machine.update(now);

#ifndef VERBOSE
    watchdog_update();
#else
    loop_timer.record_iteration_end();
#endif
  }

  return 0;
}