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

#ifndef AUDIO_MEMORY_READER_H_R6GTYSPZ
#define AUDIO_MEMORY_READER_H_R6GTYSPZ

#include <stdint.h>

struct AudioMemoryReader {
  AudioMemoryReader() : encoding(0) {};

  // Reader interface
  void init(const unsigned int *data, const uint32_t data_length);

  // Reader interface
  bool has_data() {
    return this->encoding > 0;
  }

  // Reader interface
  void read_samples(int16_t *out, const uint16_t count);

private:
  int read_next() {
    const unsigned int *end = this->beginning + this->data_length - 1;
    if (next == end) {
      encoding = 0;
      return 0;
    } else {
      const int sample = *next;
      ++next;
      return sample;
    }
  }

  const unsigned int *next;
  const unsigned int *beginning;
  uint32_t length;
  int16_t prior;
  volatile uint8_t encoding;
  uint32_t data_length;
};

#endif /* end of include guard: AUDIO_MEMORY_READER_H_R6GTYSPZ */
