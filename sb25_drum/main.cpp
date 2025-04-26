#include <cstdint>
#include <cstdio>

#include "musin/usb/usb.h"

#include "pico/stdlib.h" // for stdio_usb_init
#include "pico/time.h"   // for sleep_us

#include "midi.h"
#include "pizza_controls.h"
#include "pizza_display.h"
#include "step_sequencer.h"

static PizzaExample::PizzaDisplay pizza_display;
static StepSequencer::Sequencer<4, 8> pizza_sequencer;
static PizzaControls pizza_controls(pizza_display, pizza_sequencer);

int main() {
  stdio_usb_init();

  Musin::Usb::init();

  midi_init();

  printf(".\nPizza Example Starting...\n");

  pizza_display.init();

  pizza_controls.init();

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
