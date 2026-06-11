#ifndef DRUM_FIRMWARE_UPDATE_BUYER_H
#define DRUM_FIRMWARE_UPDATE_BUYER_H

extern "C" {
#include "pico/time.h"
}

#include "musin/hal/logger.h"

namespace drum {

// Commits ("buys") a try-before-you-buy firmware image once the system has
// proven healthy. All images are built with the TBYB flag, so this runs after
// every flash-update boot (SysEx updates, picotool loads and UF2 drag-drop
// alike). If the image crashes or hangs before the buy, the watchdog reboot
// is a normal boot and the bootrom falls back to the previous firmware.
class FirmwareUpdateBuyer {
public:
  // Healthy main-loop time required before committing.
  static constexpr uint32_t HEALTH_PERIOD_MS = 5000;

  explicit FirmwareUpdateBuyer(musin::Logger &logger);

  // True from a flash-update boot until rom_explicit_buy has succeeded.
  bool is_trial_boot() const {
    return trial_boot_pending_;
  }

  // Call from the main loop once normal operation has been reached.
  void update(absolute_time_t now);

private:
  musin::Logger &logger_;
  bool trial_boot_pending_ = false;
  absolute_time_t healthy_since_{};
  bool healthy_since_valid_ = false;
};

} // namespace drum

#endif // DRUM_FIRMWARE_UPDATE_BUYER_H
