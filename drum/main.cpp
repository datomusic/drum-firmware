#include "musin/usb/usb.h"

#include "pico/stdio_usb.h" // for stdio_usb_init
#include "pico/time.h"      // for sleep_us

#include "audio_engine.h"
#include "midi_functions.h"
#include "musin/hal/debug_utils.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_handler.h"
#include "musin/timing/tempo_multiplier.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "sequencer_controller.h"
#include "sound_router.h"

static drum::PizzaDisplay pizza_display;

static drum::AudioEngine audio_engine;
static drum::SoundRouter sound_router(audio_engine);

static musin::timing::Sequencer<4, 8> pizza_sequencer;
static musin::timing::InternalClock internal_clock(120.0f);
static musin::timing::TempoHandler tempo_handler(musin::timing::ClockSource::INTERNAL);
static musin::timing::TempoMultiplier tempo_multiplier(24, 1);

drum::SequencerController sequencer_controller(pizza_sequencer, tempo_multiplier);

static drum::PizzaControls pizza_controls(pizza_display, pizza_sequencer, internal_clock,
                                          tempo_handler, sequencer_controller, sound_router);
// TODO: Instantiate MIDIClock, ExternalSyncClock when available
// TODO: Add logic to dynamically change tempo_multiplier ratio if input PPQN changes

static musin::hal::DebugUtils::LoopTimer loop_timer(1000); // Print average loop time every 1000ms

int main() {
  stdio_usb_init();

  musin::usb::init();

  midi_init();

  // Initialize Audio Engine (stubbed for now)
  if (!audio_engine.init()) {
    //printf("Error: Failed to initialize Audio Engine!\n");
    // Potentially halt or enter a safe state
  }
  // TODO: Set initial SoundRouter output mode if needed (defaults to BOTH)
  // sound_router.set_output_mode(drum::OutputMode::AUDIO);

  pizza_display.init();

  pizza_controls.init();

  // Set the controls pointer in the sequencer controller
  sequencer_controller.set_controls_ptr(&pizza_controls);

  // --- Initialize Clocking System ---
  internal_clock.add_observer(tempo_handler);
  tempo_handler.add_observer(tempo_multiplier);
  tempo_multiplier.add_observer(sequencer_controller);

  // Connect SequencerController NoteEvents to SoundRouter
  sequencer_controller.add_observer(sound_router);

  if (tempo_handler.get_clock_source() == musin::timing::ClockSource::INTERNAL) {
    internal_clock.start();
  }
  // TODO: Add logic to register TempoHandler with other clocks
  // TODO: Add logic to start/stop clocks based on TempoHandler source selection

  while (true) {
    pizza_controls.update(); // Update controls first to get current state

    // Get state needed for display drawing from controls
    bool is_running = pizza_controls.is_running();
    float stopped_highlight_factor = pizza_controls.get_stopped_highlight_factor();

    // Draw sequencer state with the required arguments
    pizza_display.draw_sequencer_state(pizza_sequencer, sequencer_controller, is_running,
                                       stopped_highlight_factor);

    pizza_display.show();
    // Brief Delay: Important for WS2812 LED latching after show()
    sleep_us(80);

    musin::usb::background_update();
    midi_read();

    audio_engine.process();

    loop_timer.record_iteration_end();
  }

  return 0;
}
