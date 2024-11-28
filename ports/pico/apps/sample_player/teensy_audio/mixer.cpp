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

static int16_t temp_buffer[AUDIO_BLOCK_SAMPLES];

uint32_t AudioMixer4::fill_buffer(int16_t *out_samples) {
  for (int sample_index = 0; sample_index < AUDIO_BLOCK_SAMPLES;
       ++sample_index) {
    out_samples[sample_index] = 0;
  }

  for (int channel = 0; channel < source_count; ++channel) {
    sources[channel]->fill_buffer(temp_buffer);
    for (int sample_index = 0; sample_index < AUDIO_BLOCK_SAMPLES;
         ++sample_index) {
      out_samples[sample_index] =
          (out_samples[sample_index] / 2) + (temp_buffer[sample_index] / 2);
    }
  }

  return AUDIO_BLOCK_SAMPLES;
}
