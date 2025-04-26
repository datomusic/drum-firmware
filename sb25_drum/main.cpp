#include <cstdint>

#include "musin/usb/usb.h"

#include "pico/stdlib.h" // for stdio_usb_init
#include "pico/time.h"   // for sleep_us

#include "midi.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "step_sequencer.h"
#include "tempo_handler.h"
#include "internal_clock.h"
#include "tempo_multiplier.h"     // Include TempoMultiplier
#include "sequencer_controller.h" // Include SequencerController

static PizzaExample::PizzaDisplay pizza_display;
static StepSequencer::Sequencer<4, 8> pizza_sequencer;
static PizzaControls pizza_controls(pizza_display, pizza_sequencer);

// --- Clock and Tempo Setup ---
// Instantiate the internal clock (e.g., starting at 120 BPM)
static Clock::InternalClock internal_clock(120.0f);
// Instantiate the tempo handler (defaults to Internal source)
static Tempo::TempoHandler tempo_handler;
// Instantiate the tempo multiplier (default: 1/4 -> 24 PPQN output from 96 PPQN input)
static Tempo::TempoMultiplier tempo_multiplier(1, 4);
// Instantiate the sequencer controller, linking it to the sequencer data
static StepSequencer::SequencerController sequencer_controller(pizza_sequencer);
// TODO: Instantiate MIDIClock, ExternalSyncClock when available

int main() {
  stdio_usb_init();

  Musin::Usb::init();

  midi_init();


  pizza_display.init();

  pizza_controls.init();

  // --- Initialize Clocking System ---
  // Initialization logic for InternalClock is now handled in its constructor and start() method.

  // Register TempoHandler to listen to InternalClock
  internal_clock.add_observer(tempo_handler);
  // Register TempoMultiplier to listen to TempoHandler
  tempo_handler.add_observer(tempo_multiplier);
  // Register SequencerController to listen to TempoMultiplier
  tempo_multiplier.add_observer(sequencer_controller);

  // Start the internal clock if it's the default source
  if (tempo_handler.get_clock_source() == Tempo::ClockSource::INTERNAL) {
      internal_clock.start();
  }
  // TODO: Add logic to register TempoHandler with other clocks
  // TODO: Add logic to start/stop clocks based on TempoHandler source selection

  // Initialize a simple pattern
  pizza_sequencer.get_track(0).get_step(0) = {36, 100, true}; // Kick on 1
  pizza_sequencer.get_track(0).get_step(4) = {36, 100, true}; // Kick on 3
  pizza_sequencer.get_track(1).get_step(2) = {38, 100, true}; // Snare on 2
  pizza_sequencer.get_track(1).get_step(6) = {38, 100, true}; // Snare on 4


  while (true) {
    // 1. Update Controls: Read inputs, process events, update internal state,
    //    and request display changes via pizza_display methods.
    pizza_controls.update();

    // Update sequencer display (show current step)
    pizza_display.draw_sequencer_state(pizza_sequencer, sequencer_controller.get_current_step());

    // 2. Update Display: Send the buffered LED data to the hardware.
    pizza_display.show();
    // 3. Handle Background Tasks
    Musin::Usb::background_update();
    midi_read();

    // 4. Brief Delay: Important for WS2812 LED latching after show()
    sleep_us(80);
  }

  return 0;
}
