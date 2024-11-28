#ifndef BUFFER_SOURCE_H_ANRBYEKH
#define BUFFER_SOURCE_H_ANRBYEKH

#include "pico/audio.h"

struct BufferSource {
  virtual void fill_buffer(audio_buffer_t *pool) = 0;
};

#endif /* end of include guard: BUFFER_SOURCE_H_ANRBYEKH */
