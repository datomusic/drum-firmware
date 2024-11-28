#ifndef AUDIO_OUTPUT_H_5M07E3OB
#define AUDIO_OUTPUT_H_5M07E3OB

#include "pico/audio.h"

namespace AudioOutput {
typedef void (*BufferCallback)(int16_t *out_samples);

void init(BufferCallback callback);
void deinit();

audio_buffer_t *new_buffer();
} // namespace AudioOutput

#endif /* end of include guard: AUDIO_OUTPUT_H_5M07E3OB */
