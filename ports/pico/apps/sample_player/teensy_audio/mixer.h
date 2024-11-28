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

#ifndef mixer_h_
#define mixer_h_

// #include "AudioStream.h"
#include "../audio_output.h"
#include "../buffer_source.h"
#include <stdint.h>
#include <vector>

struct AudioMixer4 : BufferSource {
  AudioMixer4(BufferSource **sources, unsigned int source_count)
      : sources(sources), source_count(source_count) {

    for (int i = 0; i < 4; i++) {
      multiplier[i] = 256;
    }
  }
  uint32_t fill_buffer(int16_t *out_samples);
  void gain(unsigned int channel, float gain) {
    if (channel >= 4) {
      return;
    }

    if (gain > 127.0f) {
      gain = 127.0f;
    } else if (gain < -127.0f) {
      gain = -127.0f;
    }

    multiplier[channel] = gain * 256.0f; // TODO: proper roundoff?
  }

private:
  unsigned int source_count;
  BufferSource **sources;
  int16_t multiplier[4];
};

#endif
