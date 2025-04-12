/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Jonathan Payne (jon@jonnypayne.com)
 * Based on Effect_Fade by Paul Stoffregen

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
 Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef CRUSHER_H_4BACOXIO
#define CRUSHER_H_4BACOXIO

#include "audio_output.h"
#include "buffer_source.h"
#include <cmath> // Include for std::round

struct Crusher : BufferSource {
  Crusher(BufferSource &source) : source(source) {
  }

  void fill_buffer(AudioBlock &out_samples) {
    source.fill_buffer(out_samples);
    crush(out_samples);
  }

  void bits(uint8_t b) {
    if (b > 16)
      b = 16;
    else if (b == 0)
      b = 1;
    crushBits = b;
  }

  void sampleRate(const float hz) {
    // Clamp frequency to valid range before calculating step
    float clamped_hz = etl::clamp(hz, static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) / 64.0f,
                                  static_cast<float>(AudioOutput::SAMPLE_FREQUENCY));
    int n = static_cast<int>(std::round(static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) /
                                        clamped_hz)); // Use std::round
    // Clamp step to [1, 64]
    sampleStep = etl::clamp(n, 1, 64);
  }

  /**
   * @brief Sets the bit depth using a normalized value ("Squish").
   * Maps [0.0, 1.0] linearly to [16 bits, 1 bit].
   * 0.0 = 16 bits (no crush), 1.0 = 1 bit (max crush).
   * @param squish_normalized Value between 0.0 and 1.0. Clamped internally.
   */
  void squish(float squish_normalized) {
    float clamped_squish = etl::clamp(squish_normalized, 0.0f, 1.0f);
    // Map 0.0 -> 16, 1.0 -> 1
    float b_float = 16.0f - clamped_squish * 15.0f;
    bits(static_cast<uint8_t>(std::round(b_float))); // Use std::round
  }

  /**
   * @brief Sets the sample rate reduction using a normalized value ("Squeeze").
   * Maps [0.0, 1.0] logarithmically to [SAMPLE_FREQUENCY, SAMPLE_FREQUENCY/64].
   * 0.0 = No rate reduction, 1.0 = Max rate reduction (SAMPLE_FREQUENCY/64).
   * @param squeeze_normalized Value between 0.0 and 1.0. Clamped internally.
   */
  void squeeze(float squeeze_normalized) {
    float clamped_squeeze = etl::clamp(squeeze_normalized, 0.0f, 1.0f);
    // Logarithmic mapping: 0.0 -> SAMPLE_FREQ, 1.0 -> SAMPLE_FREQ/64
    const float min_rate = static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) / 64.0f;
    const float max_rate = static_cast<float>(AudioOutput::SAMPLE_FREQUENCY);
    const float log_min = std::log(min_rate); // Use std::log
    const float log_max = std::log(max_rate); // Use std::log
    // Inverse mapping: squeeze=0 -> log_max, squeeze=1 -> log_min
    const float log_rate = log_max - clamped_squeeze * (log_max - log_min);
    const float rate_hz = std::exp(log_rate); // Use std::exp
    sampleRate(rate_hz);
  }

private:
  void crush(AudioBlock &samples);

  BufferSource &source;
  uint8_t crushBits = 16; // 16 = off
  uint8_t sampleStep = 1; // the number of samples to double up. This simple
                          // technique only allows a few stepped positions.
};

#endif /* end of include guard: CRUSHER_H_4BACOXIO */
