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

#include "synth_dc.h"
#include "port/section_macros.h"

namespace musin::audio {

// The Teensy original writes packed 16-bit pairs through a uint32_t pointer;
// this port writes per sample. The per-sample values are identical: each
// output sample is the top half of the 32-bit magnitude accumulator.
void __time_critical_func(SynthDc::fill_buffer)(::AudioBlock &out_samples) {
  int16_t *p = out_samples.begin();
  int16_t *end = out_samples.end();
  int32_t count;

  if (state == 0) {
    // steady DC output, simply fill the buffer with fixed value
    const int16_t val = (int16_t)(magnitude >> 16);
    while (p < end) {
      *p++ = val;
    }
  } else {
    // transitioning to a new DC level
    count = substract_int32_then_divide_int32(target, magnitude, increment);
    if (count >= (int32_t)out_samples.size()) {
      // this update will not reach the target
      while (p < end) {
        magnitude += increment;
        *p++ = (int16_t)(magnitude >> 16);
      }
    } else {
      // this update reaches the target
      while (count > 0 && p < end) {
        count--;
        magnitude += increment;
        *p++ = (int16_t)(magnitude >> 16);
      }
      magnitude = target;
      state = 0;
      const int16_t val = (int16_t)(magnitude >> 16);
      while (p < end) {
        *p++ = val;
      }
    }
  }
}

} // namespace musin::audio
