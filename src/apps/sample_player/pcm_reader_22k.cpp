
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

#include "pcm_reader_22k.h"

extern "C" {
extern const int16_t ulaw_decode_table[256];
};

void PCMReader22k::reset() {
  uint32_t format;

  prior = 0;
  next = sample_data;
  format = *next++;
  beginning = next;
  remaining_length = format & 0xFFFFFF;
  encoding = format >> 24;
}

uint32_t PCMReader22k::read_samples(int16_t *out,
                                    const uint16_t max_sample_count) {
  uint32_t tmp32, consumed = 0, samples_written = 0;
  int16_t s0, s1, s2;
  int i;

  s0 = prior;

  switch (encoding) {

  case 0x82: // 16 bits PCM, 22050 Hz, Mono
    for (i = 0; i < max_sample_count; i += 4) {
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
