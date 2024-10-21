#include "audio_memory_reader.h"

extern "C" {
extern const int16_t ulaw_decode_table[256];
};

void AudioMemoryReader::play(const unsigned int *data) {
  uint32_t format;

  playing = 0;
  prior = 0;
  format = *data++;
  next = data;
  beginning = data;
  length = format & 0xFFFFFF;
  playing = format >> 24;
}

uint32_t AudioMemoryReader::read_samples(int16_t *out, const uint16_t count) {
  const unsigned int *in;
  uint32_t tmp32, consumed;
  int16_t s0, s1, s2;
  int i;

  in = next;
  s0 = prior;

  switch (playing) {
  case 0x82: // 16 bits PCM, 22050 Hz
    for (i = 0; i < count; i += 4) {
      tmp32 = *in++;
      s1 = (int16_t)(tmp32 & 65535);
      s2 = (int16_t)(tmp32 >> 16);
      *out++ = (s0 + s1) >> 1;
      *out++ = s1;
      *out++ = (s1 + s2) >> 1;
      *out++ = s2;
      s0 = s2;
    }
    consumed = count / 2;
    break;

  default:
    playing = 0;
    return 0;
  }
  prior = s0;
  next = in;

  if (consumed == 0) {
    playing = 0;
  }

  if (length > consumed) {
    length -= consumed;
  } else {
    playing = 0;
  }

  return consumed;
}
