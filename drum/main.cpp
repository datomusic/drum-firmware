#include "musin/usb/usb.h"

#include "pico/stdio_usb.h"
#include "pico/time.h"

#include "audio_engine.h"
#include "midi_functions.h"
#include "musin/hal/debug_utils.h"
#include "musin/timing/internal_clock.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/sync_out.h"
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

// Configure TempoMultiplier. If InternalClock provides TempoEvents at 96 PPQN,
// and SequencerController expects 96 PPQN, then TempoMultiplier should pass through.
// (96, 1) results in _input_ticks_per_output_tick = 1, meaning pass-through.
static musin::timing::TempoMultiplier tempo_multiplier(24, 1);

drum::SequencerController sequencer_controller(pizza_sequencer, tempo_multiplier, sound_router);

static drum::PizzaControls pizza_controls(pizza_display, pizza_sequencer,
                                          tempo_handler, sequencer_controller,
                                          sound_router);

constexpr std::uint32_t SYNC_OUT_GPIO_PIN = 3;
static musin::timing::SyncOut sync_out(SYNC_OUT_GPIO_PIN, internal_clock);

// TODO: Instantiate MIDIClock, ExternalSyncClock when available
// TODO: Add logic to dynamically change tempo_multiplier ratio if input PPQN changes

static musin::hal::DebugUtils::LoopTimer loop_timer(1000);

int main() {
  stdio_usb_init();

  musin::usb::init();

  midi_init();

  // Initialize Audio Engine (stubbed for now)
  if (!audio_engine.init()) {
    // Potentially halt or enter a safe state
  }
  // TODO: Set initial SoundRouter output mode if needed (defaults to BOTH)
  // sound_router.set_output_mode(drum::OutputMode::AUDIO);

  pizza_display.init();
  pizza_controls.init();

  // --- Initialize Clocking System ---
  internal_clock.add_observer(tempo_handler);
  tempo_handler.add_observer(tempo_multiplier);
  tempo_multiplier.add_observer(sequencer_controller);

  // SoundRouter is now called directly by SequencerController.
  // PizzaControls will no longer observe NoteEvents from SequencerController directly
  // as SequencerController no longer emits them to its own observers.
  // UI feedback for notes (e.g. drumpad fades) will be handled in PizzaControls Phase 2.

  sync_out.enable();

  if (tempo_handler.get_clock_source() == musin::timing::ClockSource::INTERNAL) {
    internal_clock.start();
  }
  // TODO: Add logic to register TempoHandler with other clocks
  // TODO: Add logic to start/stop clocks based on TempoHandler source selection

  while (true) {
    pizza_controls.update();

    pizza_display.show();
    sleep_us(280);

    musin::usb::background_update();
    midi_read();

    audio_engine.process();

    loop_timer.record_iteration_end();
  }

  return 0;
}
