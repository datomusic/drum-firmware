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

#ifndef MUSIN_AUDIO_ANALYZE_PEAK_H_
#define MUSIN_AUDIO_ANALYZE_PEAK_H_

#include "buffer_source.h"
#include "port/section_macros.h"
#include <cstdint>
#include <cstdlib>

extern "C" {
#include "hardware/sync.h"
}

namespace musin::audio {

/**
 * @brief Peak meter, ported from Teensy AudioAnalyzePeak to the pull model
 * as a pass-through tap: it forwards the upstream block unchanged while
 * recording min/max. Runs in the I2S DMA interrupt.
 */
struct AnalyzePeak : ::BufferSource {
  explicit AnalyzePeak(::BufferSource &source) : source(source) {
  }

  void __time_critical_func(fill_buffer)(::AudioBlock &out_samples) override {
    source.fill_buffer(out_samples);
    int32_t min = min_sample;
    int32_t max = max_sample;
    for (const int16_t *p = out_samples.cbegin(); p < out_samples.cend(); ++p) {
      const int16_t d = *p;
      if (d < min)
        min = d;
      if (d > max)
        max = d;
    }
    min_sample = min;
    max_sample = max;
    new_output = true;
  }

  bool available(void) {
    uint32_t saved = save_and_disable_interrupts();
    bool flag = new_output;
    if (flag)
      new_output = false;
    restore_interrupts(saved);
    return flag;
  }
  float read(void) {
    uint32_t saved = save_and_disable_interrupts();
    int min = min_sample;
    int max = max_sample;
    min_sample = 32767;
    max_sample = -32768;
    restore_interrupts(saved);
    min = std::abs(min);
    max = std::abs(max);
    if (min > max)
      max = min;
    return (float)max / 32767.0f;
  }
  float readPeakToPeak(void) {
    uint32_t saved = save_and_disable_interrupts();
    int min = min_sample;
    int max = max_sample;
    min_sample = 32767;
    max_sample = -32768;
    restore_interrupts(saved);
    return (float)(max - min) / 32767.0f;
  }

private:
  ::BufferSource &source;
  volatile bool new_output = false;
  int16_t min_sample = 32767;
  int16_t max_sample = -32768;
};

} // namespace musin::audio

#endif /* MUSIN_AUDIO_ANALYZE_PEAK_H_ */
