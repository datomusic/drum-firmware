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

#ifndef MUSIN_AUDIO_SYNTH_DC_H_
#define MUSIN_AUDIO_SYNTH_DC_H_

#include "audio_output.h"
#include "buffer_source.h"
#include <cstdint>

extern "C" {
#include "hardware/sync.h"
}

namespace musin::audio {

// compute (a - b) / c  ... handling 32 bit integer overflow without slow
// 64 bit math (portable variant of the Teensy original)
static inline int32_t substract_int32_then_divide_int32(int32_t a, int32_t b,
                                                        int32_t c) {
  uint32_t diff;
  uint8_t negative;

  if (a >= 0) {
    if (b >= 0) {
      return (a - b) / c; // no overflow if both a & b are positive
    } else {
      diff = a + (b * -1); // assumes 0x80000000 * -1 == 0x80000000
      negative = 0;
    }
  } else {
    if (b >= 0) {
      diff = (a * -1) + b; // assumes 0x80000000 * -1 == 0x80000000
      negative = 1;
    } else {
      return (a - b) / c; // no overflow if both a & b are negative
    }
  }
  if (c >= 0) {
    diff = diff / (uint32_t)c;
  } else {
    diff = diff / (uint32_t)(c * -1);
    negative ^= 1;
  }
  if (negative) {
    if (diff > 0x7FFFFFFF)
      return 0x80000000;
    return (int32_t)diff * -1;
  } else {
    if (diff > 0x7FFFFFFF)
      return 0x7FFFFFFF;
    return (int32_t)diff;
  }
}

struct SynthDc : ::BufferSource {
  SynthDc() : state(0), magnitude(0) {
  }

  // immediately jump to the new DC level
  void amplitude(float n) {
    if (n > 1.0f)
      n = 1.0f;
    else if (n < -1.0f)
      n = -1.0f;
    int32_t m = (int32_t)(n * 2147418112.0f);
    uint32_t saved = save_and_disable_interrupts();
    magnitude = m;
    state = 0;
    restore_interrupts(saved);
  }
  // slowly transition to the new DC level
  void amplitude(float n, float milliseconds) {
    if (milliseconds <= 0.0f) {
      amplitude(n);
      return;
    }
    if (n > 1.0f)
      n = 1.0f;
    else if (n < -1.0f)
      n = -1.0f;
    int32_t c =
        (int32_t)(milliseconds * (AudioOutput::SAMPLE_FREQUENCY / 1000.0f));
    if (c == 0) {
      amplitude(n);
      return;
    }
    int32_t t = (int32_t)(n * 2147418112.0f);
    uint32_t saved = save_and_disable_interrupts();
    target = t;
    if (target == magnitude) {
      state = 0;
      restore_interrupts(saved);
      return;
    }
    increment = substract_int32_then_divide_int32(target, magnitude, c);
    if (increment == 0) {
      increment = (target > magnitude) ? 1 : -1;
    }
    state = 1;
    restore_interrupts(saved);
  }
  float read(void) {
    int32_t m = magnitude;
    return (float)m * (float)(1.0 / 2147418112.0);
  }

  void fill_buffer(::AudioBlock &out_samples) override;

private:
  uint8_t state;     // 0=steady output, 1=transitioning
  int32_t magnitude; // current output
  int32_t target;    // designed output (while transitioning)
  int32_t increment; // adjustment per sample (while transitioning)
};

} // namespace musin::audio

#endif /* MUSIN_AUDIO_SYNTH_DC_H_ */
