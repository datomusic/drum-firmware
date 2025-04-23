#include <cstdint>
#include <cstdio>

#include "musin/usb/usb.h"

#include "pico/stdlib.h"
#include "pico/time.h"

#include "midi.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "sequencer.h" // Include the sequencer header

static PizzaDisplay pizza_display;
static PizzaControls pizza_controls(pizza_display);
static PizzaSequencer::Sequencer<4, 8> pizza_sequencer; // Instantiate the sequencer

int main() {
  stdio_init_all();

  Musin::Usb::init();

  midi_init();

  printf(".\nPizza Example Starting...\n");

  pizza_display.init();

  pizza_controls.init();

  // Initialize sequencer pattern (example: basic kick/snare)
  printf("Initializing sequencer pattern...\n");
  // Track 0 (Kick) - Steps 0 and 4
  pizza_sequencer.get_track(0).get_step(0) = {36, 100, true}; // Note 36, Vel 100, Enabled
  pizza_sequencer.get_track(0).get_step(4) = {36, 100, true};
  // Track 1 (Snare) - Steps 2 and 6
  pizza_sequencer.get_track(1).get_step(2) = {38, 100, true}; // Note 38, Vel 100, Enabled
  pizza_sequencer.get_track(1).get_step(6) = {38, 100, true};
  // Tracks 2 and 3 remain empty/disabled by default

  printf("Initialization complete. Entering main loop.\n");

  // --- Main Loop ---
  while (true) {
    // 1. Update Controls: Read inputs, process events, update internal state,
    //    and request display changes via pizza_display methods.
    pizza_controls.update();

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
