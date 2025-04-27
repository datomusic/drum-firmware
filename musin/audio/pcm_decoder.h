#ifndef PCM_READER_H_GB952ZMC
#define PCM_READER_H_GB952ZMC

#include "sample_reader.h"

// Reads Mono 16bit PCM samples from memory
struct PcmDecoder : SampleReader {
  constexpr PcmDecoder(const std::byte *bytes, const uint32_t byte_count) {
    set_source(bytes, byte_count);
  }

  constexpr void set_source(const std::byte *bytes, const uint32_t byte_count) {
    this->bytes = bytes;
    this->iterator = bytes;

    if (byte_count > 0) {
      this->end = bytes + (byte_count - 1);
    } else {
      this->end = this->iterator;
    }
  }

  // Reader interface
  constexpr void reset() {
    iterator = bytes;
  }

  // Reader interface
  constexpr bool has_data() {
    return iterator != nullptr;
  }

  // Reader interface
  constexpr uint32_t read_samples(AudioBlock &out_samples) {
    auto out = out_samples.begin();
    int16_t sample = 0;

    unsigned samples_written = 0;
    for (samples_written = 0; samples_written < AUDIO_BLOCK_SAMPLES; samples_written++) {
      if (!read_next(sample)) {
        break;
      }

      *out = sample;
      out++;
    }

    if (samples_written == 0) {
      iterator = nullptr;
    }

    return samples_written;
  }

private:
  constexpr bool read_next(int16_t &out) {
    if (iterator >= end) {
      return false;
    }

    const std::byte cur_bytes[2] = {(*iterator++), (*iterator++)};
    out = std::bit_cast<int16_t>(cur_bytes);
    return true;
  };

  const std::byte *bytes;
  const std::byte *iterator;
  const std::byte *end;
};

#endif /* end of include guard: PCM_READER_H_GB952ZMC */
