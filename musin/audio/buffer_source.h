#ifndef BUFFER_SOURCE_H_ANRBYEKH
#define BUFFER_SOURCE_H_ANRBYEKH

#include "block.h"
#include <stdint.h>

struct BufferSource {
  virtual uint32_t fill_buffer(AudioBlock &out_samples) = 0;
};

#endif /* end of include guard: BUFFER_SOURCE_H_ANRBYEKH */
