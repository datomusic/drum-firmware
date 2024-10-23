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

#ifndef PCM_READER_22K_H_XZWOFIKS
#define PCM_READER_22K_H_XZWOFIKS

#include "sample_reader.h"
#include <stdint.h>

struct PCMReader22k : SampleReader {
  PCMReader22k(const unsigned int *sample_data, const uint32_t data_length)
      : encoding(0), sample_data(sample_data), data_length(data_length) {};

  // Reader interface
  void reset();

  // Reader interface
  bool has_data() {
    return this->encoding > 0;
  }

  // Reader interface
  uint32_t read_samples(int16_t *out, const uint16_t count);

private:
  bool read_next(uint32_t &out) {
    const unsigned int *end = this->beginning + this->data_length - 1;
    if (next == end) {
      encoding = 0;
      return false;
    } else {
      out = *next;
      ++next;
      return true;
    }
  }

  volatile uint8_t encoding;
  const unsigned int *const sample_data;
  const uint32_t data_length;
  const unsigned int *next;
  const unsigned int *beginning;
  uint32_t remaining_length;
  int16_t prior;
};

#endif /* end of include guard: PCM_READER_22K_H_XZWOFIKS */
