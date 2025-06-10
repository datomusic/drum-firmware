#pragma once
#include "hardware/dma.h"
#include <algorithm> // For std::copy

namespace musin::hal {

struct PicoDmaCopier {
  // Constructor: Claim the DMA channel when the object is created.
  PicoDmaCopier() {
    // 'false' = don't panic if none are free, we'll handle it.
    dma_channel_ = dma_claim_unused_channel(false);
  }

  // Destructor: Release the channel when the object is destroyed.
  ~PicoDmaCopier() {
    if (dma_channel_ >= 0) {
      dma_channel_unclaim(dma_channel_);
    }
  }

  // copy is now a non-static member function.
  void copy(int16_t *dest, const int16_t *src, size_t count) const {
    if (count == 0) {
      return;
    }

    if (dma_channel_ < 0) {
      // Fallback to CPU copy if no DMA channel is available.
      std::copy(src, src + count, dest);
      return;
    }

    // The rest of the DMA logic is the same, but uses the member variable.
    dma_channel_config cfg = dma_channel_get_default_config(dma_channel_);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, true);

    dma_channel_configure(dma_channel_, &cfg, dest, src, count, true);
    dma_channel_wait_for_finish_blocking(dma_channel_);
  }

private:
  int dma_channel_ = -1;
};

} // namespace musin::hal
