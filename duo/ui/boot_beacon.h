#ifndef DUO_UI_BOOT_BEACON_H
#define DUO_UI_BOOT_BEACON_H

// Boot-progress beacon for bring-up debugging.
//
// The DUO's symptom (dark panel, no USB) is compatible with a hang anywhere in
// main(), because both channels only come alive once the main loop runs. This
// gives an out-of-band signal that depends on nothing but a GPIO: the WS2812
// data line is bit-banged with interrupts masked, so no PIO state machine, no
// DMA channel, no semaphore and no alarm pool is involved. Anything the driver
// could block on is therefore excluded.
//
// Call signal(n) with an increasing n after each init step. The number of lit
// LEDs is the last stage reached, readable with no host attached.

#include <cstdint>

extern "C" {
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/platform.h"
#include "pico/time.h"
}

namespace duo::ui::boot_beacon {

namespace detail {

constexpr uint32_t DATA_PIN = PICO_DEFAULT_WS2812_PIN;
constexpr uint32_t ENABLE_PIN = DATO_SUBMARINE_LED_ENABLE_PIN;
constexpr uint32_t NUM_LEDS = 37;

// SK6812 bit timing. Cycle counts assume clk_sys; the part tolerates ±150 ns,
// which is far wider than the gpio_put and loop overhead being ignored here.
constexpr uint32_t cycles_for_ns(uint32_t ns) {
  return static_cast<uint32_t>((static_cast<uint64_t>(SYS_CLK_HZ) * ns) /
                               1000000000ull);
}
constexpr uint32_t T0H = cycles_for_ns(300);
constexpr uint32_t T0L = cycles_for_ns(900);
constexpr uint32_t T1H = cycles_for_ns(600);
constexpr uint32_t T1L = cycles_for_ns(600);

__force_inline void send_bit(bool one) {
  gpio_put(DATA_PIN, 1);
  busy_wait_at_least_cycles(one ? T1H : T0H);
  gpio_put(DATA_PIN, 0);
  busy_wait_at_least_cycles(one ? T1L : T0L);
}

__force_inline void send_byte(uint8_t value) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    send_bit((value & mask) != 0);
  }
}

} // namespace detail

/**
 * @brief Lights the first `stage` LEDs dim white and leaves them lit.
 *
 * Takes the WS2812 data pin back from whatever owns it, so calling this after
 * the real display is running will fight with it — that is intended during a
 * bisect and the reason it is not part of DuoDisplay.
 */
inline void signal(uint8_t stage) {
  using namespace detail;

  gpio_init(ENABLE_PIN);
  gpio_set_dir(ENABLE_PIN, GPIO_OUT);
  gpio_put(ENABLE_PIN, 1);

  gpio_init(DATA_PIN);
  gpio_set_function(DATA_PIN, GPIO_FUNC_SIO);
  gpio_set_dir(DATA_PIN, GPIO_OUT);
  gpio_put(DATA_PIN, 0);
  busy_wait_us(300); // SK6812 reset: latch whatever the line was mid-frame

  const uint32_t save = save_and_disable_interrupts();
  for (uint32_t i = 0; i < NUM_LEDS; ++i) {
    const uint8_t level = (i < stage) ? 24 : 0; // GRB, dim to stay eye-safe
    send_byte(level);
    send_byte(level);
    send_byte(level);
  }
  restore_interrupts(save);

  busy_wait_us(300);
}

} // namespace duo::ui::boot_beacon

#endif // DUO_UI_BOOT_BEACON_H
