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

#include "synth_whitenoise.h"
#include "dspinst.h"
#include "port/section_macros.h"
#include <cstring>

namespace musin::audio {

// Park-Miller-Carta Pseudo-Random Number Generator
// http://www.firstpr.com.au/dsp/rand31/

void __time_critical_func(SynthNoiseWhite::fill_buffer)(
    ::AudioBlock &out_samples) {
  int32_t gain;
  uint32_t lo, hi;

  gain = level;
  if (gain == 0) {
    std::memset(out_samples.begin(), 0, out_samples.size() * sizeof(int16_t));
    return;
  }
  lo = seed;
  for (int16_t &out : out_samples) {
    hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
    lo = 16807 * (lo & 0xFFFF);
    lo += (hi & 0x7FFF) << 16;
    lo += hi >> 15;
    lo = (lo & 0x7FFFFFFF) + (lo >> 31);
    out = (int16_t)signed_multiply_32x16b(gain, lo);
  }
  seed = lo;
}

uint16_t SynthNoiseWhite::instance_count = 0;

} // namespace musin::audio
