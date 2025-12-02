#include "status_display.h"

namespace testmachine {

namespace {
constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;
}

StatusDisplay::StatusDisplay(musin::Logger &logger_ref)
    : _leds(PIZZA_LED_DATA_PIN, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS,
            DEFAULT_COLOR_CORRECTION),
      _logger_ref(logger_ref), _current_status(DisplayStatus::IDLE) {}

bool StatusDisplay::init() {
  ExternalPinState led_pin_state =
      check_external_pin_state(PIZZA_LED_DATA_PIN, _logger_ref);
  uint8_t initial_brightness = (led_pin_state == ExternalPinState::PULL_UP)
                                   ? REDUCED_BRIGHTNESS
                                   : MAX_BRIGHTNESS;
  _leds.set_brightness(initial_brightness);

  if (!_leds.init()) {
    return false;
  }

  gpio_init(PIZZA_LED_ENABLE_PIN);
  gpio_set_dir(PIZZA_LED_ENABLE_PIN, GPIO_OUT);
  gpio_put(PIZZA_LED_ENABLE_PIN, 1);

  _leds.clear();
  _leds.show();
  return true;
}

void StatusDisplay::deinit() {
  gpio_put(PIZZA_LED_ENABLE_PIN, 0);
}

void StatusDisplay::set_status(DisplayStatus status) {
  _current_status = status;
}

void StatusDisplay::update() {
  _leds.clear();

  ui::Color status_color;
  switch (_current_status) {
  case DisplayStatus::IDLE:
    status_color = COLOR_IDLE;
    break;
  case DisplayStatus::RUNNING:
    status_color = COLOR_RUNNING;
    break;
  case DisplayStatus::PASSED:
    status_color = COLOR_PASSED;
    break;
  case DisplayStatus::FAILED:
    status_color = COLOR_FAILED;
    break;
  }

  _leds.set_pixel(LED_PLAY_BUTTON, static_cast<uint32_t>(status_color));
  _leds.show();
}

} // namespace testmachine
