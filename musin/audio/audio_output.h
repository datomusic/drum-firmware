#ifndef AUDIO_OUTPUT_H_5M07E3OB
#define AUDIO_OUTPUT_H_5M07E3OB

#include "buffer_source.h"

namespace AudioOutput {
static const int SAMPLE_FREQUENCY = 44100;

bool init();
bool update(BufferSource &source, uint8_t volume);
void deinit();

} // namespace AudioOutput

#endif /* end of include guard: AUDIO_OUTPUT_H_5M07E3OB */
