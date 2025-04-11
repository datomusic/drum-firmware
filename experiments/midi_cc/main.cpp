// static_example_main.cpp
#include "pico/stdlib.h"
#include <cstdint>
#include <cstdio>
#include "pico/time.h"
#include <array>

// Include the specific MIDI observer implementation for this experiment
#include "midi_cc_observer.h"
// Include the core AnalogControl class from the musin library
#include "musin/ui/analog_control.h" 
using Musin::UI::AnalogControl;

extern "C" {
  #include "hardware/adc.h"
  #include "hardware/gpio.h"
}

constexpr auto PIN_ADDR_0 = 29;
constexpr auto PIN_ADDR_1 = 6;
constexpr auto PIN_ADDR_2 = 7;
constexpr auto PIN_ADDR_3 = 9;

constexpr auto PIN_ADC = 28;

constexpr unsigned int PIN_RING_1 = 15;
constexpr unsigned int PIN_RING_2 = 14;
constexpr unsigned int PIN_RING_3 = 13;
constexpr unsigned int PIN_RING_4 = 11;
constexpr unsigned int PIN_RING_5 = 10;

// Static array for multiplexer address pins
const std::array<std::uint32_t, 4> address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2, PIN_ADDR_3};
const std::array<std::uint32_t, 5> columns_pins = {PIN_RING_1, PIN_RING_2, PING_RING_3, PIN_RING_4, PIN_RING_5};

void send_midi_cc([[maybe_unused]] uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("%x:%3d\n", cc_number, value);
}

// Define MIDI observers statically
// These will be allocated at compile time
static MIDICCObserver cc_observers[] = {
  {16, 0, send_midi_cc},  // CC 16, channel 1
  {17, 0, send_midi_cc},  // CC 17, channel 1
  {18, 0, send_midi_cc},  // CC 18, channel 1
  {19, 0, send_midi_cc},  // CC 19, channel 1
  {20, 0, send_midi_cc},  // CC 20, channel 1
  {21, 0, send_midi_cc},  // CC 21, channel 1
  {22, 0, send_midi_cc},  // CC 22, channel 1
  {23, 0, send_midi_cc},  // CC 23, channel 1
  {24, 0, send_midi_cc}   // CC 24, channel 1
};

// Statically allocate multiplexed controls using the class from musin::ui
static AnalogControl<1> mux_controls[8] = {
  {10, PIN_ADC, address_pins, 3 }, // ID 10, Mux Channel 3
  {11, PIN_ADC, address_pins, 4 }, // ID 11, Mux Channel 4
  {12, PIN_ADC, address_pins, 8 }, // ID 12, Mux Channel 8
  {13, PIN_ADC, address_pins, 15 }, // ID 13, Mux Channel 15
  {14, PIN_ADC, address_pins, 0 }, // ID 14, Mux Channel 0 (Example)
  {15, PIN_ADC, address_pins, 1 }, // ID 15, Mux Channel 1 (Example)
  {16, PIN_ADC, address_pins, 2 }, // ID 16, Mux Channel 2 (Example)
  {17, PIN_ADC, address_pins, 5 }  // ID 17, Mux Channel 5 (Example) - Corrected index
};


int main() {
  // Initialize system
  stdio_init_all();
  
  sleep_ms(1000);

  printf("MIDI CC demo");

  for (int i = 0; i < 8; i++) {
    mux_controls[i].init();
    mux_controls[i].add_observer(&cc_observers[i]);
  }

  while (true) {
      // Update all mux controls
      for (auto& control : mux_controls) {
          control.update();
      }
      // Add a small delay to avoid oversampling
      sleep_ms(1);
  }
  
  return 0;
}
