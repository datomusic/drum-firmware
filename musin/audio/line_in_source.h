#ifndef MUSIN_AUDIO_LINE_IN_SOURCE_H
#define MUSIN_AUDIO_LINE_IN_SOURCE_H

#include "musin/audio/block.h"
#include "musin/audio/buffer_source.h"
#include "port/section_macros.h"
#include <atomic>
#include <cstddef>

namespace musin::audio {

/**
 * @brief Minimal interface for a non-blocking block-oriented sample reader.
 */
struct BlockReader {
  /**
   * @brief Reads up to out.size() mono samples into the block.
   * @param out The block to fill from the start.
   * @return The number of samples written (may be 0 when no data is
   * available).
   */
  virtual size_t read_samples(AudioBlock &out) = 0;
};

/**
 * @brief Adapts a BlockReader (e.g. the line input) to the BufferSource
 * interface used by the audio chain.
 *
 * Samples that the reader cannot provide are filled with silence, so a
 * starved or disabled input never stalls the audio pipeline.
 *
 * The gate allows several LineInSource instances to share one reader while
 * only the enabled one drains it; a disabled instance outputs silence
 * without touching the reader. fill_buffer runs in the I2S DMA interrupt
 * and must stay RAM-resident.
 */
class LineInSource : public ::BufferSource {
public:
  explicit LineInSource(BlockReader &reader) : reader_(reader) {
  }

  void set_enabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_relaxed);
  }

  void __time_critical_func(fill_buffer)(::AudioBlock &out_samples) override {
    size_t count = 0;
    if (enabled_.load(std::memory_order_relaxed)) {
      count = reader_.read_samples(out_samples);
    }
    for (size_t i = count; i < out_samples.size(); ++i) {
      out_samples[i] = 0;
    }
  }

private:
  BlockReader &reader_;
  std::atomic<bool> enabled_{false};
};

} // namespace musin::audio

#endif // MUSIN_AUDIO_LINE_IN_SOURCE_H
