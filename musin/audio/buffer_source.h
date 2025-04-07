#ifndef BUFFER_SOURCE_H_ANRBYEKH
#define BUFFER_SOURCE_H_ANRBYEKH

#include "block.h"

struct BufferSource {
  virtual void fill_buffer(AudioBlock &out_samples) = 0;
};

#endif /* end of include guard: BUFFER_SOURCE_H_ANRBYEKH */
