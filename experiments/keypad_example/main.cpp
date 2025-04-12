// In your main C++ file (e.g., main.cpp)

#include "musin/ui/keypad_hc138.h" // Include the driver header
#include <array>                   // Needed for std::array
#include <pico/stdlib.h>
#include <stdio.h>

// --- Define your keypad configuration ---
constexpr std::uint8_t NUM_ROWS = 7;
constexpr std::uint8_t NUM_COLS = 8;
constexpr size_t KEY_COUNT = NUM_ROWS * NUM_COLS;

// GPIO pins connected to HC138 address lines A0, A1, A2
const std::array<uint, 3> DECODER_ADDR_PINS = {10, 11, 12}; // Example GPIOs

// GPIO pins connected to keypad columns
const uint COL_PINS[NUM_COLS] = {9, 8, 7, 6, 5, 4, 3, 2}; // Example GPIOs - C-style array

// --- Allocate the state buffer ---
// IMPORTANT: This buffer MUST exist for the lifetime of the keypad object.
// Global or static allocation is usually safest.
Musin::UI::KeyData key_state_buffer[KEY_COUNT];

// --- Create the keypad object ---
// Pass pointers to the pin arrays and the state buffer
Musin::UI::Keypad_HC138 keypad(NUM_ROWS, NUM_COLS, DECODER_ADDR_PINS,
                               COL_PINS,         // Pass the C-style array (decays to pointer)
                               key_state_buffer, // Pass the state buffer
                               10000,            // Scan interval: 10ms
                               8000,             // Debounce time: 8ms
                               400000            // Hold time: 400ms
);

int main() {
  stdio_init_all();

  sleep_ms(2000);

  printf("Pico Keypad HC138 Driver Example (No Malloc)\n");

  keypad.init(); // Initialize GPIOs

  printf("Keypad initialized. Starting scan loop...\n");

  while (true) {
    // Call scan() periodically.
    if (keypad.scan()) {
      // A scan was performed, check for key events

      for (std::uint8_t r = 0; r < keypad.get_num_rows(); ++r) {
        for (std::uint8_t c = 0; c < keypad.get_num_cols(); ++c) {

          if (keypad.was_pressed(r, c)) {
            printf("Key Pressed:  (%u, %u)\n", r, c);
          }
          if (keypad.was_released(r, c)) {
            printf("Key Released: (%u, %u)\n", r, c);
          }
          // Check for hold *after* press, usually only want one message
          else if (keypad.is_held(r, c)) {
            // Optional: Add logic to only print the hold message once per hold period
            // This simple check will print repeatedly while held.
            // printf("Key Held:   (%u, %u)\n", r, c);
          }
        }
      }
    }

    // Allow other tasks or sleep briefly
    sleep_ms(1);
  }

  return 0; // Never reached
}