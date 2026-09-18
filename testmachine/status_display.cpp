#include "status_display.h"

namespace testmachine {

namespace {
constexpr uint32_t DEFAULT_COLOR_CORRECTION = 0xffe080;
}

StatusDisplay::StatusDisplay(musin::Logger &logger_ref)
    : _leds(PIZZA_LED_DATA_PIN, musin::drivers::RGBOrder::GRB, MAX_BRIGHTNESS,
            DEFAULT_COLOR_CORRECTION),
      _logger_ref(logger_ref) {}

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

void StatusDisplay::register_test(const char *test_name, uint8_t step,
                                   uint8_t track) {
  TestName name(test_name);
  _test_mappings[name] = TestLedMapping{step, track};
  _test_statuses[name] = DisplayStatus::IDLE;
}

void StatusDisplay::set_status(const char *test_name, DisplayStatus status) {
  TestName name(test_name);
  auto it = _test_statuses.find(name);
  if (it != _test_statuses.end()) {
    it->second = status;
  }
}

uint32_t StatusDisplay::get_led_index(uint8_t step, uint8_t track) const {
  if (step < 1 || step > 8 || track < 1 || track > 4) {
    return LED_PLAY_BUTTON;
  }
  uint32_t array_index = (step - 1) * 4 + (track - 1);
  return LED_ARRAY[array_index];
}

void StatusDisplay::update() {
  _leds.clear();

  for (const auto &pair : _test_statuses) {
    const TestName &test_name = pair.first;
    DisplayStatus status = pair.second;

    auto mapping_it = _test_mappings.find(test_name);
    if (mapping_it == _test_mappings.end()) {
      continue;
    }

    const TestLedMapping &mapping = mapping_it->second;
    uint32_t led_index = get_led_index(mapping.step, mapping.track);

    ui::Color status_color;
    switch (status) {
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

    _leds.set_pixel(led_index, static_cast<uint32_t>(status_color));
  }

  _leds.show();
}

} // namespace testmachine
