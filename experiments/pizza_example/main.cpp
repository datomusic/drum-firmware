#include <cstdio>
#include <cstdint>

#include "musin/usb/usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "midi.h"
#include "pizza_display.h"
#include "pizza_controls.h"

static PizzaDisplay pizza_display;
static PizzaControls pizza_controls(pizza_display);

int main() {
  stdio_init_all();
  Musin::Usb::init();
  midi_init();

  printf(".\nPizza Example Starting...\n");
  sleep_ms(1000);

  // Initialize display (init() is now void, error handling is internal or uses panic)
  pizza_display.init();

  pizza_controls.init();

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
