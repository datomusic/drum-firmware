
/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "audio_memory_reader.h"

extern "C" {
extern const int16_t ulaw_decode_table[256];
};

void AudioMemoryReader::reset() {
  uint32_t format;

  prior = 0;
  next = sample_data;
  format = *next++;
  beginning = next;
  remaining_length = format & 0xFFFFFF;
  encoding = format >> 24;
}

constexpr uint32_t AudioMemoryReader::read_samples(AudioBlock &out_samples) {
  uint32_t tmp32, consumed = 0, samples_written = 0;
  int16_t s0, s1, s2, s3, s4;
  int i;

  s0 = prior;

  auto out = out_samples.begin();

  switch (encoding) {
  case 0x01: // u-law encoded, 44100 Hz, Mono
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 4) {
      if (!read_next(tmp32)) {
        break;
      }
      *out++ = ulaw_decode_table[(tmp32 >> 0) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 8) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 16) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 24) & 255];

      samples_written += 4;
      consumed += 4;
    }
    break;

  case 0x81: // 16 bit PCM, 44100 Hz, Mono
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 2) {
      if (!read_next(tmp32)) {
        break;
      }
      *out++ = (int16_t)(tmp32 & 65535);
      *out++ = (int16_t)(tmp32 >> 16);

      samples_written += 2;
      consumed += 2;
    }
    break;

  case 0x02: // u-law encoded, 22050 Hz, Mono
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 8) {
      if (!read_next(tmp32)) {
        break;
      }
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

      consumed += 4;
      samples_written += 8;
    }
    break;

  case 0x82: // 16 bits PCM, 22050 Hz, Mono
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 4) {
      if (!read_next(tmp32)) {
        break;
      }
      s1 = (int16_t)(tmp32 & 65535);
      s2 = (int16_t)(tmp32 >> 16);
      *out++ = (s0 + s1) >> 1;
      *out++ = s1;
      *out++ = (s1 + s2) >> 1;
      *out++ = s2;

      s0 = s2;

      consumed += 2;
      samples_written += 4;
    }
    break;

  case 0x03: // u-law encoded, 11025 Hz, Mono
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 16) {
      if (!read_next(tmp32)) {
        break;
      }
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

      consumed += 4;
      samples_written += 16;
    }
    break;

  case 0x83: // 16 bit PCM, 11025 Hz, Mono
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 8) {
      if (!read_next(tmp32)) {
        break;
      }
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

      consumed += 2;
      samples_written += 8;
    }
    break;

  default:
    encoding = 0;
    return samples_written;
  }

  prior = s0;

  if (consumed == 0 || samples_written == 0) {
    encoding = 0;
  } else {

    if (remaining_length > consumed) {
      remaining_length -= consumed;
    } else {
      encoding = 0;
    }
  }

  return samples_written;
}
