#ifndef TESTMACHINE_STATUS_DISPLAY_H
#define TESTMACHINE_STATUS_DISPLAY_H

#include "drum/drum_pizza_hardware.h"
#include "musin/drivers/ws2812-dma.h"
#include "musin/hal/logger.h"
#include "ui/color.h"
#include <cstdint>

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

class StatusDisplay {
public:
  static constexpr uint8_t MAX_BRIGHTNESS = 255;
  static constexpr uint8_t REDUCED_BRIGHTNESS = 100;

  static constexpr ui::Color COLOR_OFF = ui::Color(0x000000);
  static constexpr ui::Color COLOR_IDLE = ui::Color(0x101010);
  static constexpr ui::Color COLOR_RUNNING = ui::Color(0x0000FF);
  static constexpr ui::Color COLOR_PASSED = ui::Color(0x00FF00);
  static constexpr ui::Color COLOR_FAILED = ui::Color(0xFF0000);

  explicit StatusDisplay(musin::Logger &logger_ref);

  StatusDisplay(const StatusDisplay &) = delete;
  StatusDisplay &operator=(const StatusDisplay &) = delete;

  bool init();
  void deinit();
  void set_status(DisplayStatus status);
  void update();

private:
  musin::drivers::WS2812_DMA<NUM_LEDS> _leds;
  musin::Logger &_logger_ref;
  DisplayStatus _current_status;
};

} // namespace testmachine

#endif // TESTMACHINE_STATUS_DISPLAY_H
