// static_example_main.cpp
#include "pico/stdlib.h"
#include <cstdint>
#include <cstdio>
#include "pico/time.h"
#include <array>

#include "analog_control.h"
#include "musin/hal/analog_in.h"

extern "C" {
  #include "hardware/adc.h"
  #include "hardware/gpio.h"
}

constexpr auto PIN_ADDR_0 = 29;
constexpr auto PIN_ADDR_1 = 6;
constexpr auto PIN_ADDR_2 = 7;
constexpr auto PIN_ADDR_3 = 9;

constexpr auto PIN_ADC = 28;

// Static array for multiplexer address pins
const std::array<std::uint32_t, 4> address_pins = {PIN_ADDR_0, PIN_ADDR_1, PIN_ADDR_2, PIN_ADDR_3};

// static Musin::HAL::AnalogInMux<4> POT1(PIN_ADC, address_pins, 3);
// static Musin::HAL::AnalogInMux<4> POT3(PIN_ADC, address_pins, 4);
// static Musin::HAL::AnalogInMux<4> POT5(PIN_ADC, address_pins, 8);
// static Musin::HAL::AnalogInMux<4> POT7(PIN_ADC, address_pins, 15);

void send_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value) {
  printf("%x:%3d\n", cc_number, value);
}

// Define MIDI observers statically
// These will be allocated at compile time
static Musin::Controller::MIDICCObserver cc_observers[] = {
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

// Statically allocate multiplexed controls
static Musin::Controller::AnalogControl<1> mux_controls[8] = {
  {10, PIN_ADC, address_pins, 3 },
  {11, PIN_ADC, address_pins, 4 },
  {12, PIN_ADC, address_pins, 8 },
  {13, PIN_ADC, address_pins, 15 },
  {14, PIN_ADC, address_pins, 4 },
  {15, PIN_ADC, address_pins, 5 },
  {16, PIN_ADC, address_pins, 6 },
  {17, PIN_ADC, address_pins, 7 }
};


int main() {
  // Initialize system
  stdio_init_all();
  
  sleep_ms(1000);

  printf("MIDI CC demo");

  for (int i = 0; i < 8; i++) {
    mux_controls[i].init();
    mux_controls[i].add_observer(&cc_observers[i+1]);
  }

  // Main control loop
  while (true) {
      // Update direct control
      // direct_control.update();
      
      // Update all mux controls
      // 
      // Update all mux controls
      for (auto& control : mux_controls) {
          control.update();
      }
      // Add a small delay to avoid oversampling
      sleep_ms(1);
  }
  
  return 0;
}
