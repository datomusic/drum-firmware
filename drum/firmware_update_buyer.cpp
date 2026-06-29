#include "drum/firmware_update_buyer.h"

extern "C" {
#include "pico/bootrom.h"
}

namespace drum {

namespace {
// rom_explicit_buy needs a word-aligned scratch buffer of at least one flash
// sector to rewrite the sector holding the image's TBYB flag.
alignas(4) uint8_t buy_workspace[4096];
} // namespace

FirmwareUpdateBuyer::FirmwareUpdateBuyer(musin::Logger &logger)
    : logger_(logger) {
  boot_info_t boot_info{};
  if (rom_get_boot_info(&boot_info) != 0 &&
      (boot_info.tbyb_and_update_info &
       BOOT_TBYB_AND_UPDATE_FLAG_BUY_PENDING)) {
    trial_boot_pending_ = true;
    logger_.info("FirmwareUpdateBuyer: Buy pending, partition",
                 static_cast<int32_t>(boot_info.partition));
  }
}

void FirmwareUpdateBuyer::update(absolute_time_t now) {
  if (!trial_boot_pending_) {
    return;
  }

  if (!healthy_since_valid_) {
    healthy_since_ = now;
    healthy_since_valid_ = true;
    return;
  }

  if (absolute_time_diff_us(healthy_since_, now) <
      static_cast<int64_t>(HEALTH_PERIOD_MS) * 1000) {
    return;
  }

  const int result = rom_explicit_buy(buy_workspace, sizeof(buy_workspace));
  if (result == 0) {
    logger_.info("FirmwareUpdateBuyer: Firmware committed.");
    trial_boot_pending_ = false;
  } else {
    logger_.error("FirmwareUpdateBuyer: explicit_buy failed",
                  static_cast<int32_t>(result));
    // Retry from a fresh health period rather than hammering the bootrom.
    healthy_since_valid_ = false;
  }
}

} // namespace drum
