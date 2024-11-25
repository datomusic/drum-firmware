#ifndef AUDIO_OUTPUT_H_5M07E3OB
#define AUDIO_OUTPUT_H_5M07E3OB

#include "pico/audio.h"

namespace AudioOutput {
typedef void (*BufferCallback)(audio_buffer_pool_t *);

void init(BufferCallback callback, const uint32_t samples_per_buffer);
void deinit();
} // namespace AudioOutput

#endif /* end of include guard: AUDIO_OUTPUT_H_5M07E3OB */
