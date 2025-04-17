#include <cstdio>
#include <cstdint>

#include "musin/usb/usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "midi.h" // For midi_init, midi_read
#include "pizza_display.h"
#include "pizza_controls.h"

// --- Global Application Objects ---
// These are static to ensure they exist for the lifetime of the application
static PizzaDisplay pizza_display;
static PizzaControls pizza_controls(pizza_display); // Pass display reference

// --- Main Entry Point ---
int main() {
  // Basic System Initialization
  stdio_init_all();
  Musin::Usb::init();
  midi_init(); // Initialize MIDI handling (callbacks, etc.)

  printf(".\nPizza Example Starting...\n");
  sleep_ms(1000); // Pause for user terminal connection

  // Initialize Hardware Abstractions
  if (!pizza_display.init()) {
      printf("FATAL: PizzaDisplay initialization failed!\n");
      // Consider a panic or error state here
      while(true) { tight_loop_contents(); }
  }

  pizza_controls.init(); // Initialize keypad, drumpads, analog controls

  printf("Initialization complete. Entering main loop.\n");

  // --- Main Loop ---
  while (true) {
    // 1. Update Controls: Read inputs, process events, update internal state,
    //    and request display changes via pizza_display methods.
    pizza_controls.update();

    // 2. Update Display: Send the buffered LED data to the hardware.
    pizza_display.show();

    // 3. Handle Background Tasks
    Musin::Usb::background_update(); // Service TinyUSB tasks
    midi_read();                     // Process incoming MIDI messages

    // 4. Brief Delay: Important for WS2812 LED latching after show()
    sleep_us(100);
  }

  // Should theoretically never reach here
  return 0;
}
