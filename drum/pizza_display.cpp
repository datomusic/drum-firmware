#include "pizza_display.h"
#include "drum_pizza_hardware.h"

#include <algorithm>
#include <array>
#include <cstddef>

extern "C" {
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdio.h>
}

namespace drum {

namespace { // Anonymous namespace for internal helpers

constexpr auto PULL_CHECK_DELAY_US = 10;
// Note: MAX_BRIGHTNESS is defined in the header (pizza_display.h)
constexpr uint8_t REDUCED_BRIGHTNESS = 100;
constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;

enum class ExternalPinState {
  FLOATING,
  PULL_UP,
  PULL_DOWN,
  UNDETERMINED
};

ExternalPinState check_external_pin_state(std::uint32_t gpio, const char *name) {
  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);

  gpio_disable_pulls(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool initial_read = gpio_get(gpio);

  gpio_pull_up(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool pullup_read = gpio_get(gpio);

  gpio_pull_down(gpio);
  sleep_us(PULL_CHECK_DELAY_US);
  bool pulldown_read = gpio_get(gpio);

  ExternalPinState determined_state;
  const char *state_str;

  if (!initial_read && pullup_read && !pulldown_read) {
    determined_state = ExternalPinState::FLOATING;
    state_str = "Floating";
  } else if (initial_read && pullup_read && !pulldown_read) {
    determined_state = ExternalPinState::FLOATING;
    state_str = "Floating";
  } else if (!initial_read && !pullup_read) {
    determined_state = ExternalPinState::PULL_DOWN;
    state_str = "External Pull-down";
  } else if (initial_read && pulldown_read) {
    determined_state = ExternalPinState::PULL_UP;
    state_str = "External Pull-up";
  } else {
    determined_state = ExternalPinState::UNDETERMINED;
    state_str = "Undetermined / Inconsistent Reads";
  }

  // printf("PizzaDisplay Init: Pin %lu (%s) external state check result: %s\n", gpio, name,
  // state_str);

  gpio_disable_pulls(gpio);
  sleep_us(PULL_CHECK_DELAY_US);

  return determined_state;
}

} // anonymous namespace

PizzaDisplay::PizzaDisplay()
    : _leds(PIN_LED_DATA, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS, DEFAULT_COLOR_CORRECTION),
      note_colors({0xFF0000, 0xFF0020, 0xFF0040, 0xFF0060, 0xFF1010, 0xFF1020, 0xFF2040,
                   0xFF2060, 0x0000FF, 0x0028FF, 0x0050FF, 0x0078FF, 0x1010FF, 0x1028FF,
                   0x2050FF, 0x3078FF, 0x00FF00, 0x00FF1E, 0x00FF3C, 0x00FF5A, 0x10FF10,
                   0x10FF1E, 0x10FF3C, 0x20FF5A, 0xFFFF00, 0xFFE100, 0xFFC300, 0xFFA500,
                   0xFFFF20, 0xFFE120, 0xFFC320, 0xFFA520}),
      _track_override_colors{} {
}

bool PizzaDisplay::init() {
  // printf("PizzaDisplay: Initializing LEDs...\n");

  // Check LED data pin state to determine initial brightness. Pullup = SK6812, pulldown = SK6805
  ExternalPinState led_pin_state = check_external_pin_state(PIN_LED_DATA, "LED_DATA");
  uint8_t initial_brightness =
      (led_pin_state == ExternalPinState::PULL_UP) ? REDUCED_BRIGHTNESS : MAX_BRIGHTNESS;
  // printf("PizzaDisplay: Setting initial LED brightness to %u (based on pin state: %d)\n",
  //        initial_brightness, static_cast<int>(led_pin_state));
  _leds.set_brightness(initial_brightness);

  if (!_leds.init()) {
    // printf("Error: Failed to initialize WS2812 LED driver!\n");
    return false;
  }

  gpio_init(PIN_LED_ENABLE);
  gpio_set_dir(PIN_LED_ENABLE, GPIO_OUT);
  gpio_put(PIN_LED_ENABLE, 1);
  clear();
  show();
  // printf("PizzaDisplay: Initialization Complete.\n");
  return true;
}

void PizzaDisplay::show() {
  _leds.show();
}

void PizzaDisplay::set_brightness(uint8_t brightness) {
  _leds.set_brightness(brightness);
  // Note: Brightness only affects subsequent set_pixel calls in the current WS2812 impl.
  // If immediate effect is desired, the buffer would need to be recalculated.
}

void PizzaDisplay::clear() {
  _leds.clear();
}

void PizzaDisplay::set_led(uint32_t index, uint32_t color) {
  if (index < NUM_LEDS) {
    _leds.set_pixel(index, color);
  }
}

void PizzaDisplay::set_play_button_led(uint32_t color) {
  _leds.set_pixel(LED_PLAY_BUTTON, color);
}

uint32_t PizzaDisplay::get_note_color(uint8_t note_index) const {
  if (note_index < note_colors.size()) {
    return note_colors[note_index];
  }
  return 0;
}

std::optional<uint32_t> PizzaDisplay::get_drumpad_led_index(uint8_t pad_index) const {
  switch (pad_index) {
  case 0:
    return LED_DRUMPAD_1;
  case 1:
    return LED_DRUMPAD_2;
  case 2:
    return LED_DRUMPAD_3;
  case 3:
    return LED_DRUMPAD_4;
  default:
    return std::nullopt; // Invalid pad index
  }
}

void PizzaDisplay::set_keypad_led(uint8_t row, uint8_t col, uint8_t intensity) {
  std::optional<uint32_t> led_index_opt = get_keypad_led_index(row, col);

  if (led_index_opt.has_value()) {
    uint32_t color = calculate_intensity_color(intensity);
    _leds.set_pixel(led_index_opt.value(), color);
  }
}

void PizzaDisplay::set_track_override_color(uint8_t track_index, uint32_t color) {
  if (track_index < _track_override_colors.size()) {
    _track_override_colors[track_index] = color;
  }
}

void PizzaDisplay::clear_track_override_color(uint8_t track_index) {
  if (track_index < _track_override_colors.size()) {
    _track_override_colors[track_index] = std::nullopt;
  }
}

void PizzaDisplay::clear_all_track_override_colors() {
  for (size_t i = 0; i < _track_override_colors.size(); ++i) {
    _track_override_colors[i] = std::nullopt;
  }
}

} // namespace drum
