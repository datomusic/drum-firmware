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

#include "mixer.h"
#include "dspinst.h"
#include <stdint.h>

uint32_t AudioMixer4::fill_buffer(int16_t *out_buffer) {
  /*
        // int32_t mult = multiplier[0];
      sources[0]->fill_buffer(out_buffer);
      for (int channel = 1; channel < source_count; ++channel) {
        sources[channel]->fill_buffer(temp_buffer);
        // TODO: Actually apply gain on first channel

        int32_t *out_samples = (int32_t *)out_buffer->buffer->bytes;
        for (int i = 0; i < temp_buffer->sample_count; ++i) {
          // const int32_t mult = multiplier[channel];

          // Actually apply gain, and fix distortion
          int32_t *temp_samples = (int32_t *)temp_buffer->buffer->bytes;
          int32_t val = (out_samples[i] / 2) + (temp_samples[i] / 2);
          out_samples[i] = val;
        }
      }
    */
}
