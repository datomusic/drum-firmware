#ifndef PCM_READER_H_GB952ZMC
#define PCM_READER_H_GB952ZMC

#include "sample_reader.h"

// Reads Mono 16bit PCM samples from memory
struct PcmDecoder : SampleReader {
  constexpr PcmDecoder(const unsigned char *bytes, const uint32_t byte_count) {
    set_source(bytes, byte_count);
  }

  constexpr void set_source(const unsigned char *bytes, const uint32_t byte_count) {
    this->read_pos = 0;
    this->bytes = bytes;
    this->byte_count = byte_count;
  }

  // Reader interface
  constexpr void reset() {
    read_pos = 0;
  }

  // Reader interface
  constexpr bool has_data() {
    return read_pos < byte_count;
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
      read_pos = byte_count;
    }

    return samples_written;
  }

private:
  constexpr bool read_next(int16_t &out) {
    if (read_pos > byte_count - 2) {
      return false;
    }

    const unsigned char cur_byte[2] = {bytes[read_pos], bytes[read_pos+1]};
    out = std::bit_cast<int16_t>(cur_byte);
    read_pos += 2;
    return true;
  };

  const unsigned char *bytes;
  uint32_t byte_count;
  uint32_t read_pos;
  // const int16_t *iterator;
};

#endif /* end of include guard: PCM_READER_H_GB952ZMC */
