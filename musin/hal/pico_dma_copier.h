#pragma once
#include "hardware/dma.h"
#include <algorithm> // For std::copy

namespace musin::hal {

struct PicoDmaCopier {
  // init() must be called once at startup from non-ISR context.
  static void init() {
    // 'true' = panic if no channel is available. This is safe at startup.
    dma_channel_ = dma_claim_unused_channel(true);
  }

  // deinit() can be called at shutdown.
  static void deinit() {
    if (dma_channel_ >= 0) {
      dma_channel_unclaim(dma_channel_);
      dma_channel_ = -1;
    }
  }

  // copy is now a static member function.
  static void copy(int16_t *dest, const int16_t *src, size_t count) {
    if (count == 0) {
      return;
    }

    if (dma_channel_ < 0) {
      // Fallback to CPU copy if init() was not called or failed.
      std::copy(src, src + count, dest);
      return;
    }

    // The DMA logic now uses the static channel.
    dma_channel_config cfg = dma_channel_get_default_config(dma_channel_);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, true);

    dma_channel_configure(dma_channel_, &cfg, dest, src, count, true);
    dma_channel_wait_for_finish_blocking(dma_channel_);
  }

private:
  inline static int dma_channel_ = -1;
};

} // namespace musin::hal
