#ifndef MUSIN_AUDIO_LINE_IN_SOURCE_H
#define MUSIN_AUDIO_LINE_IN_SOURCE_H

#include "musin/audio/block.h"
#include "musin/audio/buffer_source.h"
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
 */
class LineInSource : public ::BufferSource {
public:
  explicit LineInSource(BlockReader &reader) : reader_(reader) {
  }

  void fill_buffer(::AudioBlock &out_samples) override {
    const size_t count = reader_.read_samples(out_samples);
    for (size_t i = count; i < out_samples.size(); ++i) {
      out_samples[i] = 0;
    }
  }

private:
  BlockReader &reader_;
};

} // namespace musin::audio

#endif // MUSIN_AUDIO_LINE_IN_SOURCE_H
