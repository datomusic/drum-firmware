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

#ifndef filter_variable_h_
#define filter_variable_h_

#include "audio_output.h"
#include <cmath> // Include for std::log, std::exp
#include <cstdint>
#include <etl/math.h>

namespace musin::audio {

struct Filter {
public:
  // --- Public Constants for parameter scaling and limits ---
  static constexpr float PI_F = 3.141592654f;
  static constexpr float Q31_FLOAT_SCALE = 2147483647.0f; // 2^31 - 1

  static constexpr float MIN_AUDIBLE_FREQ_HZ = 20.0f;
  static constexpr float MAX_FREQ_NYQUIST_DIVISOR = 2.5f; // Limits freq to SAMPLE_RATE / 2.5

  static constexpr float MIN_RESONANCE_Q = 0.7f;
  static constexpr float MAX_RESONANCE_Q = 5.0f;

  static constexpr float MIN_OCTAVE_CONTROL = 0.0f;
  static constexpr float MAX_OCTAVE_CONTROL = 6.9999f;       // Approx 7 octaves
  static constexpr float OCTAVE_CONTROL_INT_SCALE = 4096.0f; // For Q12 representation

  struct Outputs {
    AudioBlock lowpass;
    AudioBlock bandpass;
    AudioBlock highpass;
  };

  Filter() {
    // Initialize with default values by calling the setters
    frequency(1000.0f);   // Default frequency
    octave_control(1.0f); // default values
    resonance(0.707f);    // Default resonance (Butterworth)
    state_inputprev = 0;
    state_lowpass = 0;
    state_bandpass = 0;
  }

  void frequency(float freq) {
    if (freq < MIN_AUDIBLE_FREQ_HZ)
      freq = MIN_AUDIBLE_FREQ_HZ;
    else if (freq > static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) / MAX_FREQ_NYQUIST_DIVISOR)
      freq = static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) / MAX_FREQ_NYQUIST_DIVISOR;

    const float radians_per_sample_half = PI_F / static_cast<float>(AudioOutput::SAMPLE_FREQUENCY);
    setting_fcenter = (freq * (radians_per_sample_half / 2.0f)) * Q31_FLOAT_SCALE;
    // TODO: should we use an approximation when freq is not a const,
    // so the sinf() function isn't linked?
    setting_fmult = std::sin(freq * (radians_per_sample_half / 2.0f)) * Q31_FLOAT_SCALE;
  }

  void resonance(float q) {
    if (q < MIN_RESONANCE_Q)
      q = MIN_RESONANCE_Q;
    else if (q > MAX_RESONANCE_Q)
      q = MAX_RESONANCE_Q;
    // TODO: allow lower Q when frequency is lower
    // 1073741824.0f is 2^30, for Q30 representation of (1/q)
    setting_damp = (1.0f / q) * (1 << 30); // Q30
  }

  void octave_control(float n) {
    // filter's corner frequency is Fcenter * 2^(control * N)
    // where "control" ranges from -1.0 to +1.0
    // and "N" allows the frequency to change from 0 to 7 octaves
    if (n < MIN_OCTAVE_CONTROL)
      n = MIN_OCTAVE_CONTROL;
    else if (n > MAX_OCTAVE_CONTROL)
      n = MAX_OCTAVE_CONTROL;
    setting_octavemult = n * OCTAVE_CONTROL_INT_SCALE; // Q12
  }

  void update_variable(const AudioBlock &input_samples, const AudioBlock &control,
                       Outputs &outputs);
  void update_fixed(const AudioBlock &input_samples, Outputs &outputs);

  /**
   * @brief Sets the filter cutoff/center frequency using a normalized value.
   * Maps [0.0, 1.0] logarithmically to the audible range [20Hz, SAMPLE_FREQUENCY/2.5Hz].
   * @param freq_normalized Value between 0.0 and 1.0. Clamped internally.
   */
  void frequency_normalized(float freq_normalized) {
    calculate_frequency(etl::clamp(freq_normalized, 0.0f, 1.0f));
  }

  /**
   * @brief Sets the filter resonance (Q) using a normalized value.
   * Maps [0.0, 1.0] linearly to the typical range [0.7, 5.0].
   * @param res_normalized Value between 0.0 and 1.0. Clamped internally.
   */
  void resonance_normalized(float res_normalized) {
    calculate_resonance(etl::clamp(res_normalized, 0.0f, 1.0f));
  }

private:
  // Internal frequency calculation based on normalized input
  void calculate_frequency(float freq_normalized) {
    // Logarithmic mapping: 0.0 -> MIN_AUDIBLE_FREQ_HZ, 1.0 -> SAMPLE_FREQ /
    // MAX_FREQ_NYQUIST_DIVISOR
    const float max_freq = static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) /
                           MAX_FREQ_NYQUIST_DIVISOR; // Ensure float division
    const float log_min = std::log(MIN_AUDIBLE_FREQ_HZ);
    const float log_max = std::log(max_freq);
    const float log_freq = log_min + freq_normalized * (log_max - log_min);
    const float freq_hz = std::exp(log_freq); // Use std::exp
    frequency(freq_hz);                       // Call the original frequency setter
  }
  // Internal resonance calculation based on normalized input
  void calculate_resonance(float res_normalized) {
    // Linear mapping: 0.0 -> MIN_RESONANCE_Q, 1.0 -> MAX_RESONANCE_Q
    const float q = MIN_RESONANCE_Q + res_normalized * (MAX_RESONANCE_Q - MIN_RESONANCE_Q);
    resonance(q); // Call the original resonance setter
  }

  int32_t setting_fcenter;
  int32_t setting_fmult;
  int32_t setting_octavemult;
  int32_t setting_damp;
  int32_t state_inputprev;
  int32_t state_lowpass;
  int32_t state_bandpass;
};

struct Lowpass : BufferSource {
  Lowpass(BufferSource &from) : from(from) {
  }

  void fill_buffer(AudioBlock &out_samples) {
    from.fill_buffer(out_samples);
    filter.update_fixed(out_samples, outputs);
    etl::copy(outputs.lowpass.cbegin(), outputs.lowpass.cend(), out_samples.begin());
  }

  /**
   * @brief Sets the filter cutoff frequency using a normalized value [0.0, 1.0].
   */
  void frequency(float freq_normalized) {
    filter.frequency_normalized(freq_normalized);
  }

  /**
   * @brief Sets the filter resonance using a normalized value [0.0, 1.0].
   */
  void resonance(float res_normalized) {
    filter.resonance_normalized(res_normalized);
  }

  BufferSource &from;
  Filter::Outputs outputs;
  Filter filter;
};

struct Highpass : BufferSource {
  Highpass(BufferSource &from) : from(from) {
  }

  void fill_buffer(AudioBlock &out_samples) {
    from.fill_buffer(out_samples);
    filter.update_fixed(out_samples, outputs);
    etl::copy(outputs.highpass.cbegin(), outputs.highpass.cend(), out_samples.begin());
  }

  /**
   * @brief Sets the filter cutoff frequency using a normalized value [0.0, 1.0].
   */
  void frequency(float freq_normalized) {
    filter.frequency_normalized(freq_normalized);
  }

  /**
   * @brief Sets the filter resonance using a normalized value [0.0, 1.0].
   */
  void resonance(float res_normalized) {
    filter.resonance_normalized(res_normalized);
  }

  BufferSource &from;
  Filter::Outputs outputs;
  Filter filter;
};

} // namespace musin::audio

#endif
