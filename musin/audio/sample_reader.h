#ifndef READER_H_IOD4JYAV
#define READER_H_IOD4JYAV

#include "block.h"
#include <stdint.h>

struct SampleReader {
  virtual void reset() = 0;
  virtual bool has_data() = 0;

  // Should always fill the output block with number of samples: AUDIO_BLOCK_SAMPLES
  virtual uint32_t read_samples(AudioBlock &out) = 0;

  // Reads the next single sample.
  // Returns true if a sample was read, false if no more data is available.
  virtual bool read_next(int16_t &out) = 0;
};

#endif /* end of include guard: READER_H_IOD4JYAV */
