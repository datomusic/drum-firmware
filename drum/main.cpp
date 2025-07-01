#include "drum/note_event_queue.h"
#include "musin/hal/debug_utils.h"
#include "musin/hal/logger.h"
#include "musin/midi/midi_input_queue.h"
#include "musin/midi/midi_output_queue.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/sync_out.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include "drum/configuration_manager.h"
#include "drum/sysex_file_handler.h"
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
#include "midi_functions.h"
#include "pizza_controls.h"
#include "sequencer_controller.h"

#ifdef VERBOSE
static musin::PicoLogger logger(musin::LogLevel::DEBUG);
static musin::hal::DebugUtils::LoopTimer loop_timer(10000);
#else
static musin::NullLogger logger;
#endif

// Model
static drum::NoteEventQueue note_event_queue;
static drum::ConfigurationManager config_manager(logger);
static drum::SampleRepository sample_repository(logger);
static drum::SysExFileHandler sysex_file_handler(config_manager, sample_repository, logger);
static drum::AudioEngine audio_engine(sample_repository, logger);
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::MidiClockProcessor midi_clock_processor;
static musin::timing::TempoHandler
    tempo_handler(internal_clock, midi_clock_processor,
                  drum::config::SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER,
                  musin::timing::ClockSource::INTERNAL);
static drum::SequencerController<drum::config::NUM_TRACKS, drum::config::NUM_STEPS_PER_TRACK>
    sequencer_controller(tempo_handler, note_event_queue);

static drum::MessageRouter message_router(audio_engine, sequencer_controller, note_event_queue);

// View
static drum::PizzaDisplay pizza_display(sequencer_controller, tempo_handler, logger);

// Controller
static drum::PizzaControls pizza_controls(pizza_display, tempo_handler, sequencer_controller,
                                          message_router, logger);

static musin::timing::SyncOut sync_out(DATO_SUBMARINE_SYNC_OUT_PIN, internal_clock);

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

    // Print config.json contents for debugging
    logger.info("\n--- Contents of /config.json ---");
    FILE *configFile = fopen("/config.json", "r");
    if (configFile) {
      char read_buffer[129];
      size_t bytes_read;
      while ((bytes_read = fread(read_buffer, 1, sizeof(read_buffer) - 1, configFile)) > 0) {
        read_buffer[bytes_read] = '\0';
        logger.info(read_buffer);
      }
      fclose(configFile);
      logger.info("\n--- End of /config.json ---");
    } else {
      logger.warn("Could not open /config.json to display.");
    }

    if (config_manager.load()) {
      sample_repository.load_from_config(config_manager.get_sample_configs());
    }
    // If config fails to load, sample_repository will just be empty.
  }

  midi_init(midi_clock_processor, sysex_file_handler, logger);

  audio_engine.init();
  message_router.set_output_mode(drum::OutputMode::BOTH);

  pizza_display.init();
  pizza_controls.init();

  // --- Initialize Clocking System ---
  // TempoHandler's constructor calls set_clock_source, which handles initial observation.
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(pizza_display); // PizzaDisplay needs tempo events for pulsing

  // NoteEvents are now sent from SequencerController to a queue, and processed by MessageRouter.
  // The direct observer link is removed to break the synchronous chain.

  // Register observers for SysEx state changes
  sysex_file_handler.add_observer(message_router);
  sysex_file_handler.add_observer(pizza_display);
  sysex_file_handler.add_observer(sequencer_controller);

  // Register PizzaDisplay as an observer of NoteEvents from MessageRouter
  message_router.add_observer(pizza_display);

  sync_out.enable();

  while (true) {
    sysex_file_handler.update(get_absolute_time());

    pizza_controls.update();
    sequencer_controller.update(); // Checks if a step is due and queues NoteEvents
    message_router.update();       // Drains NoteEvent queue, sending to observers and MIDI
    audio_engine.process();

    pizza_display.update(get_absolute_time());

    musin::usb::background_update();
    midi_process_input();
    tempo_handler.update();
    musin::midi::process_midi_output_queue();

#ifndef VERBOSE
    // Watchdog update for Release builds
    watchdog_update();
#else
    loop_timer.record_iteration_end();
#endif

    // yield some time
    sleep_us(10);
  }

  return 0;
}
