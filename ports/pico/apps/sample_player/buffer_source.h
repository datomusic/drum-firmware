#ifndef BUFFER_SOURCE_H_ANRBYEKH
#define BUFFER_SOURCE_H_ANRBYEKH

#include "pico/audio.h"

struct BufferSource {
  virtual uint32_t fill_buffer(int16_t *out_samples) = 0;
};

#endif /* end of include guard: BUFFER_SOURCE_H_ANRBYEKH */
