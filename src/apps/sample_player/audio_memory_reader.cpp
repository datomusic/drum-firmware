#include "audio_memory_reader.h"

extern "C" {
extern const int16_t ulaw_decode_table[256];
};

void AudioMemoryReader::play(const unsigned int *data) {
  uint32_t format;

  prior = 0;
  format = *data++;
  next = data;
  beginning = data;
  length = format & 0xFFFFFF;
  encoding = format >> 24;
}

void AudioMemoryReader::read_samples(int16_t *out, const uint16_t count) {
  const unsigned int *in;
  uint32_t tmp32, consumed;
  int16_t s0, s1, s2, s3, s4;
  int i;

  in = next;
  s0 = prior;

  switch (encoding) {
  case 0x01: // u-law encoded, 44100 Hz
    for (i = 0; i < count; i += 4) {
      tmp32 = *in++;
      *out++ = ulaw_decode_table[(tmp32 >> 0) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 8) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 16) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 24) & 255];
    }
    consumed = count;
    break;

  case 0x81: // 16 bit PCM, 44100 Hz
    for (i = 0; i < count; i += 2) {
      tmp32 = *in++;
      *out++ = (int16_t)(tmp32 & 65535);
      *out++ = (int16_t)(tmp32 >> 16);
    }
    consumed = count;
    break;

  case 0x02: // u-law encoded, 22050 Hz
    for (i = 0; i < count; i += 8) {
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
    }
    consumed = count / 2;
    break;

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

  case 0x03: // u-law encoded, 11025 Hz
    for (i = 0; i < count; i += 16) {
      tmp32 = *in++;
      s1 = ulaw_decode_table[(tmp32 >> 0) & 255];
      s2 = ulaw_decode_table[(tmp32 >> 8) & 255];
      s3 = ulaw_decode_table[(tmp32 >> 16) & 255];
      s4 = ulaw_decode_table[(tmp32 >> 24) & 255];
      *out++ = (s0 * 3 + s1) >> 2;
      *out++ = (s0 + s1) >> 1;
      *out++ = (s0 + s1 * 3) >> 2;
      *out++ = s1;
      *out++ = (s1 * 3 + s2) >> 2;
      *out++ = (s1 + s2) >> 1;
      *out++ = (s1 + s2 * 3) >> 2;
      *out++ = s2;
      *out++ = (s2 * 3 + s3) >> 2;
      *out++ = (s2 + s3) >> 1;
      *out++ = (s2 + s3 * 3) >> 2;
      *out++ = s3;
      *out++ = (s3 * 3 + s4) >> 2;
      *out++ = (s3 + s4) >> 1;
      *out++ = (s3 + s4 * 3) >> 2;
      *out++ = s4;
      s0 = s4;
    }
    consumed = count / 4;
    break;

  case 0x83: // 16 bit PCM, 11025 Hz
    for (i = 0; i < count; i += 8) {
      tmp32 = *in++;
      s1 = (int16_t)(tmp32 & 65535);
      s2 = (int16_t)(tmp32 >> 16);
      *out++ = (s0 * 3 + s1) >> 2;
      *out++ = (s0 + s1) >> 1;
      *out++ = (s0 + s1 * 3) >> 2;
      *out++ = s1;
      *out++ = (s1 * 3 + s2) >> 2;
      *out++ = (s1 + s2) >> 1;
      *out++ = (s1 + s2 * 3) >> 2;
      *out++ = s2;
      s0 = s2;
    }
    consumed = count / 4;
    break;

  default:
    encoding = 0;
    return;
  }

  prior = s0;
  next = in;

  if (consumed == 0) {
    encoding = 0;
  }

  if (length > consumed) {
    length -= consumed;
  } else {
    encoding = 0;
  }
}
