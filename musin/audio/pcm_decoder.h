#ifndef PCM_READER_H_GB952ZMC
#define PCM_READER_H_GB952ZMC

#include "sample_reader.h"

// Reads Mono 16bit PCM samples from memory
struct PcmDecoder : SampleReader {
  constexpr PcmDecoder(const unsigned char *bytes, const uint32_t byte_count) {
    set_source(bytes, byte_count);
  }

  constexpr void set_source(const unsigned char *bytes, const uint32_t byte_count) {
    this->bytes = bytes;
    this->end = this->bytes + byte_count;
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
    unsigned char tmp1;
    unsigned char tmp2;

    unsigned samples_written = 0;
    for (samples_written = 0; samples_written < AUDIO_BLOCK_SAMPLES; samples_written++) {
      if (!read_next(tmp1)) {
        break;
      }

      if (!read_next(tmp2)) {
        break;
      }

      const uint16_t upper = tmp2 << 8;
      *out++ = (upper | tmp1);
    }

    if (samples_written == 0) {
      iterator = nullptr;
    }

    return samples_written;
  }

private:
  constexpr bool read_next(unsigned char &out) {
    if (iterator == nullptr) {
      return false;
    }

    if (iterator == end) {
      iterator = nullptr;
      return false;
    } else {
      out = *iterator;
      ++iterator;
      return true;
    }
  };

  const unsigned char *bytes;
  const unsigned char *end;
  const unsigned char *iterator;
};

#endif /* end of include guard: PCM_READER_H_GB952ZMC */
