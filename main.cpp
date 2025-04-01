#include "pico/stdlib.h"
#include "ws2812.pio.h"

#define WS2812_PIN 2
#define NUM_PIXELS 1

int main() {
    // Initialize the PIO program
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    uint sm = pio_claim_unused_sm(pio, true);
    
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);

    while (true) {
        // Set LED to red
        pio_sm_put_blocking(pio, sm, 0x00FF00);
        sleep_ms(500);
        
        // Set LED to green
        pio_sm_put_blocking(pio, sm, 0xFF0000);
        sleep_ms(500);
        
        // Set LED to blue
        pio_sm_put_blocking(pio, sm, 0x0000FF);
        sleep_ms(500);
    }
}
