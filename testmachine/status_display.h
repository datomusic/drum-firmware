#ifndef TESTMACHINE_STATUS_DISPLAY_H
#define TESTMACHINE_STATUS_DISPLAY_H

#include "drum/drum_pizza_hardware.h"
#include "musin/drivers/ws2812-dma.h"
#include "musin/hal/logger.h"
#include "ui/color.h"
#include <cstdint>
#include <etl/map.h>
#include <etl/string.h>

extern "C" {
#include "hardware/gpio.h"
}

namespace testmachine {

enum class DisplayStatus {
  IDLE,
  RUNNING,
  PASSED,
  FAILED
};

struct TestLedMapping {
  uint8_t step;
  uint8_t track;
};

class StatusDisplay {
public:
  static constexpr uint8_t MAX_BRIGHTNESS = 255;
  static constexpr uint8_t REDUCED_BRIGHTNESS = 100;
  static constexpr uint8_t MAX_TESTS = 8;
  static constexpr uint8_t MAX_TEST_NAME_LENGTH = 32;

  static constexpr ui::Color COLOR_OFF = ui::Color(0x000000);
  static constexpr ui::Color COLOR_IDLE = ui::Color(0x101010);
  static constexpr ui::Color COLOR_RUNNING = ui::Color(0x0000FF);
  static constexpr ui::Color COLOR_PASSED = ui::Color(0x00FF00);
  static constexpr ui::Color COLOR_FAILED = ui::Color(0xFF0000);

  using TestName = etl::string<MAX_TEST_NAME_LENGTH>;

  explicit StatusDisplay(musin::Logger &logger_ref);

  StatusDisplay(const StatusDisplay &) = delete;
  StatusDisplay &operator=(const StatusDisplay &) = delete;

  bool init();
  void deinit();
  void register_test(const char *test_name, uint8_t step, uint8_t track);
  void set_status(const char *test_name, DisplayStatus status);
  void update();

private:
  uint32_t get_led_index(uint8_t step, uint8_t track) const;

  musin::drivers::WS2812_DMA<NUM_LEDS> _leds;
  musin::Logger &_logger_ref;
  etl::map<TestName, TestLedMapping, MAX_TESTS> _test_mappings;
  etl::map<TestName, DisplayStatus, MAX_TESTS> _test_statuses;
};

} // namespace testmachine

#endif // TESTMACHINE_STATUS_DISPLAY_H
