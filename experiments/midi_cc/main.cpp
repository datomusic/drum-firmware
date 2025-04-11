// static_example_main.cpp
#include "pico/stdlib.h"
#include <cstdint>
#include <cstdio>
#include "pico/time.h"
#include <array>

extern "C" {
  #include "hardware/adc.h"
  #include "hardware/gpio.h"
}

constexpr auto PIN_ADDR_0 = 29;
constexpr auto PIN_ADDR_1 = 6;
constexpr auto PIN_ADDR_2 = 7;
constexpr auto PIN_ADDR_3 = 9;

constexpr auto PIN_ADC = 28;

int main() {
  // Initialize system
  stdio_init_all();
  
  sleep_ms(1000);

  printf("MIDI CC demo");
  
  // Initialize all controls
  // direct_control.init();
  // direct_control.add_observer(&cc_observers[0]);
  
  adc_init();
  adc_gpio_init(PIN_ADC);

  gpio_init(PIN_ADDR_0);
  gpio_set_dir(PIN_ADDR_0, GPIO_OUT);
  gpio_put(PIN_ADDR_0, 0);

  gpio_init(PIN_ADDR_1);
  gpio_set_dir(PIN_ADDR_1, GPIO_OUT);
  gpio_put(PIN_ADDR_1, 0);

  gpio_init(PIN_ADDR_2);
  gpio_set_dir(PIN_ADDR_2, GPIO_OUT);
  gpio_put(PIN_ADDR_2, 0);

  gpio_init(PIN_ADDR_3);
  gpio_set_dir(PIN_ADDR_3, GPIO_OUT);
  gpio_put(PIN_ADDR_3, 0);
  
  // Main control loop
  while (true) {
      // Update direct control
      // direct_control.update();
      
      // Update all mux controls
      adc_select_input(2);
      for(uint8_t address = 0; address < 16; address++) {
        gpio_put(PIN_ADDR_0, (address & 1));
        gpio_put(PIN_ADDR_1, ((address >> 1) & 1));
        gpio_put(PIN_ADDR_2, ((address >> 2) & 1));
        gpio_put(PIN_ADDR_3, ((address >> 3) & 1));
        sleep_us(10);
        printf("%x:%3d  ", address, adc_read()>>3);
      }

      
      // Add a small delay to avoid oversampling
      sleep_ms(50);
      printf("\n");
  }
  
  return 0;
}
