#include "musin/hal/debug_utils.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/sync_out.h"
#include "musin/timing/tempo_handler.h"
#include "musin/usb/usb.h"

#include "pico/stdio_usb.h"
#include "pico/time.h"

#include "audio_engine.h"
#include "config.h"
#include "midi_functions.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "sequencer_controller.h"
#include "sound_router.h"
#include "musin/midi/midi_message_queue.h" // For processing MIDI output queue

#include "musin/timing/midi_clock_processor.h" // Added for MidiClockProcessor

// Model
static drum::AudioEngine audio_engine;
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::MidiClockProcessor midi_clock_processor; // Instantiate MidiClockProcessor
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

// TODO: Instantiate MIDIClock, ExternalSyncClock when available

static musin::hal::DebugUtils::LoopTimer loop_timer(1000);

int main() {
  stdio_usb_init();

  musin::usb::init();

  // Pass sound_router, sequencer_controller, and midi_clock_processor
  midi_init(sound_router, sequencer_controller, midi_clock_processor); 

  if (!audio_engine.init()) {
    // Potentially halt or enter a safe state
  }
  // TODO: Set initial SoundRouter output mode if needed (defaults to BOTH)
  // sound_router.set_output_mode(drum::OutputMode::AUDIO);

  pizza_display.init();
  pizza_controls.init();

  // --- Initialize Clocking System ---
  internal_clock.add_observer(tempo_handler);
  midi_clock_processor.add_observer(tempo_handler); // TempoHandler observes MidiClockProcessor
  tempo_handler.add_observer(sequencer_controller);
  tempo_handler.add_observer(pizza_display); // PizzaDisplay needs tempo events for pulsing

  // Register SoundRouter and PizzaDisplay as observers of NoteEvents from SequencerController
  sequencer_controller.add_observer(sound_router);
  sequencer_controller.add_observer(pizza_display);

  // Register PizzaDisplay and AudioEngine as observers of NoteEvents from SoundRouter
  sound_router.add_observer(pizza_display);
  sound_router.add_observer(audio_engine);

  sync_out.enable();

  // The initial clock starting is now handled by TempoHandler's constructor
  // via its call to set_clock_source.
  // No need for:
  // if (tempo_handler.get_clock_source() == musin::timing::ClockSource::INTERNAL) {
  //   internal_clock.start();
  // }

  // TODO: Add logic to register TempoHandler with other clocks (e.g. ExternalSyncClock)
  //       and update TempoHandler to manage them if necessary.
  // TODO: The TempoHandler::set_clock_source method now manages starting/stopping
  //       the internal clock. Similar logic would be needed for other clock types
  //       if they require explicit start/stop from TempoHandler.

  while (true) {
    pizza_controls.update();
    audio_engine.process();

    // Update time-based animations (e.g., drumpad fades)
    pizza_display.draw_animations(get_absolute_time());
    pizza_display.draw_base_elements();
    pizza_display.show();

    musin::usb::background_update();
    midi_read();
    musin::midi::process_midi_output_queue(); // Process the outgoing MIDI queue

    loop_timer.record_iteration_end();
  }

  return 0;
}
