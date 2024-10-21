#include "audio_memory_reader.h"

extern "C" {
extern const int16_t ulaw_decode_table[256];
};

void AudioMemoryReader::read_samples8(int16_t *out) {
  const uint8_t consumed = 8;

  const unsigned int *in;
  uint32_t tmp32;
  int16_t s0, s1, s2, s3, s4;
  int i;

  if (!encoding) {
    return;
  }

  in = next;
  s0 = prior;

  switch (encoding) {
  case 0x02: // u-law encoded, 22050 Hz
             // for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 8) {
    tmp32 = *in++;
    s1 = ulaw_decode_table[(tmp32 >> 0) & 255];
    s2 = ulaw_decode_table[(tmp32 >> 8) & 255];
    s3 = ulaw_decode_table[(tmp32 >> 16) & 255];
    s4 = ulaw_decode_table[(tmp32 >> 24) & 255];
    *out++ = (s0 + s1) >> 1;
    *out++ = s1;
    *out++ = (s1 + s2) >> 1;
    *out++ = s2;
    *out++ = (s2 + s3) >> 1;
    *out++ = s3;
    *out++ = (s3 + s4) >> 1;
    *out++ = s4;
    s0 = s4;
    // }
    break;

  case 0x82: // 16 bits PCM, 22050 Hz
    for (i = 0; i < 8; i += 4) {
      tmp32 = *in++;
      s1 = (int16_t)(tmp32 & 65535);
      s2 = (int16_t)(tmp32 >> 16);
      *out++ = (s0 + s1) >> 1;
      *out++ = s1;
      *out++ = (s1 + s2) >> 1;
      *out++ = s2;
      s0 = s2;
    }
    break;

  default:
    encoding = 0;
    return;
  }
  prior = s0;
  next = in;
  if (length > consumed) {
    length -= consumed;
  } else {
    encoding = 0;
  }
}
