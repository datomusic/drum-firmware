#include <cstdint>
#include <cstdio>

#include "musin/usb/usb.h"

#include "pico/stdlib.h" // for stdio_usb_init
#include "pico/time.h"   // for sleep_us

#include "midi.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "step_sequencer.h"
#include "tempo_handler.h"
#include "internal_clock.h" // Include the new clock header

static PizzaExample::PizzaDisplay pizza_display;
static StepSequencer::Sequencer<4, 8> pizza_sequencer;
static PizzaControls pizza_controls(pizza_display, pizza_sequencer);

// --- Clock and Tempo Setup ---
// Instantiate the internal clock (e.g., starting at 120 BPM)
static Clock::InternalClock internal_clock(120.0f);
// Instantiate the tempo handler (defaults to Internal source)
static Tempo::TempoHandler tempo_handler;
// TODO: Instantiate MIDIClock, ExternalSyncClock when available
// TODO: Instantiate TempoMultiplier and register it with TempoHandler

int main() {
  stdio_usb_init();

  Musin::Usb::init();

  midi_init();

  printf(".\nPizza Example Starting...\n");

  pizza_display.init();

  pizza_controls.init();

  // --- Initialize Clocking System ---
  printf("Initializing Clocking System...\n");
  if (!internal_clock.init()) {
      printf("FATAL: Failed to initialize Internal Clock!\n");
      // Handle error appropriately, maybe halt or enter safe mode
      while(true) { tight_loop_contents(); }
  }
  // Register TempoHandler to listen to InternalClock's ticks
  internal_clock.add_observer(tempo_handler);
  printf("TempoHandler registered with InternalClock.\n");

  // Start the internal clock if it's the default source
  if (tempo_handler.get_clock_source() == Tempo::ClockSource::INTERNAL) {
      internal_clock.start();
  }
  // TODO: Add logic to register TempoHandler with other clocks
  // TODO: Add logic to start/stop clocks based on TempoHandler source selection

  printf("Initializing sequencer pattern...\n");
  pizza_sequencer.get_track(0).get_step(0) = {36, 100, true};
  pizza_sequencer.get_track(0).get_step(4) = {36, 100, true};
  pizza_sequencer.get_track(1).get_step(2) = {38, 100, true};
  pizza_sequencer.get_track(1).get_step(6) = {38, 100, true};

  printf("Initialization complete. Entering main loop.\n");

  while (true) {
    // 1. Update Controls: Read inputs, process events, update internal state,
    //    and request display changes via pizza_display methods.
    pizza_controls.update();

    // Update sequencer display
    pizza_display.draw_sequencer_state(pizza_sequencer);

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
