#ifndef READER_H_IOD4JYAV
#define READER_H_IOD4JYAV

#include <stdint.h>
#include "block.h"

struct SampleReader {
  virtual void reset() = 0;
  virtual bool has_data() = 0;

  virtual uint32_t read_samples(AudioBlock &out) = 0;
};

#endif /* end of include guard: READER_H_IOD4JYAV */
