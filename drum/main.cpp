#include "musin/hal/debug_utils.h"
#include "musin/midi/midi_message_queue.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/midi_clock_processor.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/sync_out.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include "drum/applications/rompler/standard_file_ops.h"
#include "drum/configuration_manager.h"
#include "musin/filesystem/filesystem.h"
#include "sample_repository.h"
#include "sysex/protocol.h"

#include "musin/boards/dato_submarine.h" // For pin definitions
#include "musin/drivers/aic3204.hpp"     // For the codec driver

#include "pico/stdio_usb.h"
#include "pico/time.h"

#include "audio_engine.h"
#include "config.h"
#include "midi_functions.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "sequencer_controller.h"
#include "sound_router.h"

// Hardware Drivers
static musin::drivers::Aic3204 codec(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, 100'000U,
                                     DATO_SUBMARINE_CODEC_RESET_PIN);

// Headphone jack detection state
static absolute_time_t last_headphone_check = nil_time;
constexpr uint32_t HEADPHONE_POLL_INTERVAL_MS = 100;

// SysEx File Transfer
static StandardFileOps file_ops;
static sysex::Protocol<StandardFileOps> syx_protocol(file_ops);
static bool new_file_received = false;

// Model
static drum::ConfigurationManager config_manager;
static drum::SampleRepository sample_repository;
static drum::AudioEngine audio_engine(sample_repository);
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::MidiClockProcessor midi_clock_processor;
static musin::timing::TempoHandler tempo_handler(internal_clock, midi_clock_processor,
                                                 musin::timing::ClockSource::INTERNAL);

// SequencerController needs to be declared before SoundRouter if SoundRouter depends on it.
drum::SequencerController<drum::config::NUM_TRACKS, drum::config::NUM_STEPS_PER_TRACK>
    sequencer_controller(tempo_handler);

static drum::SoundRouter sound_router(audio_engine, sequencer_controller);

// View
static drum::PizzaDisplay pizza_display(sequencer_controller, tempo_handler);

// Controller
static drum::PizzaControls pizza_controls(pizza_display, tempo_handler, sequencer_controller,
                                          sound_router);

constexpr std::uint32_t SYNC_OUT_GPIO_PIN = 3;
static musin::timing::SyncOut sync_out(SYNC_OUT_GPIO_PIN, internal_clock);

static musin::hal::DebugUtils::LoopTimer loop_timer(1000);

void on_file_received_callback() {
  new_file_received = true;
}

int main() {
  stdio_usb_init();

  musin::usb::init(true); // Wait for serial connection

  if (!musin::filesystem::init(false)) {
    // Filesystem is not critical for basic operation if no samples are present,
    // but we should log the failure.
    printf("WARNING: Failed to initialize filesystem.\n");
  } else {
    if (config_manager.load()) {
      sample_repository.load_from_config(config_manager.get_sample_configs());
    }
    // If config fails to load, sample_repository will just be empty.
  }

  midi_init(sound_router, sequencer_controller, midi_clock_processor, syx_protocol,
            on_file_received_callback);

  if (!codec.is_initialized()) {
    // This is a critical hardware failure.
    // In a real product, we might blink an LED error code.
    // For development, panicking is the clearest way to signal the issue.
    panic("Failed to initialize AIC3204 codec\n");
  }

  if (!audio_engine.init(codec)) {
    // Potentially halt or enter a safe state
    panic("Failed to initialize audio engine\n");
  }
  sound_router.set_output_mode(drum::OutputMode::BOTH);

  pizza_display.init();
  pizza_controls.init();

  // --- Initialize Clocking System ---
  // TempoHandler's constructor calls set_clock_source, which handles initial observation.
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(pizza_display); // PizzaDisplay needs tempo events for pulsing

  // Register SoundRouter and PizzaDisplay as observers of NoteEvents from SequencerController
  sequencer_controller.add_observer(sound_router);
  sequencer_controller.add_observer(pizza_display);

  // Register PizzaDisplay and AudioEngine as observers of NoteEvents from SoundRouter
  sound_router.add_observer(pizza_display);
  sound_router.add_observer(audio_engine);

  sync_out.enable();

  // Initial clock source (INTERNAL by default) is started by TempoHandler's constructor.

  // TODO: Add logic to register TempoHandler with other clocks (e.g. ExternalSyncClock)
  //       and update TempoHandler to manage them if necessary.
  // TODO: The TempoHandler::set_clock_source method now manages starting/stopping
  //       the internal clock. Similar logic would be needed for other clock types
  //       if they require explicit start/stop from TempoHandler.

  while (true) {
    // --- SysEx State Check ---
    if (syx_protocol.busy()) {
      // The protocol is actively receiving a file.
      // We could add visual feedback here, e.g., pulse a specific LED.
    } else if (new_file_received) {
      printf("Main loop: New file received, reloading configuration.\n");
      if (config_manager.load()) {
        sample_repository.load_from_config(config_manager.get_sample_configs());
      }
      new_file_received = false; // Reset the flag
    }

    pizza_controls.update();
    audio_engine.process();

    // Update time-based animations (e.g., drumpad fades)
    pizza_display.draw_animations(get_absolute_time());
    pizza_display.draw_base_elements();
    pizza_display.show();

    musin::usb::background_update();
    midi_read();                              // TODO: turn this into a musin input queue
    tempo_handler.update();                   // Call TempoHandler update for auto-switching logic
    musin::midi::process_midi_output_queue(); // Process the outgoing MIDI queue

    // Poll for headphone jack status
    if (time_reached(last_headphone_check)) {
      last_headphone_check = make_timeout_time_ms(HEADPHONE_POLL_INTERVAL_MS);
      codec.update_headphone_detection();
    }

    loop_timer.record_iteration_end();
  }

  return 0;
}
