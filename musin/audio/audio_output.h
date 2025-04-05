#ifndef AUDIO_OUTPUT_H_5M07E3OB
#define AUDIO_OUTPUT_H_5M07E3OB

#include "pico/audio.h"

namespace AudioOutput {
  static const int SAMPLE_FREQUENCY = 44100;

typedef void (*BufferCallback)(audio_buffer_t *);

bool init();
bool update(AudioOutput::BufferCallback callback);
void deinit();

} // namespace AudioOutput

#endif /* end of include guard: AUDIO_OUTPUT_H_5M07E3OB */
