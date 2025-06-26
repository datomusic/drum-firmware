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

/**
 * @file mixer.h
 * @brief Defines the AudioMixer template class for mixing multiple audio sources.
 */

#ifndef mixer_h_
#define mixer_h_

#include "buffer_source.h"
#include "dspinst.h" // For signed_saturate_rshift
#include "etl/array.h"
#include <cstddef> // For size_t
#include <stdint.h>

namespace musin::audio {

/**
 * @brief Mixes multiple audio sources into a single output buffer.
 *
 * This template class takes a fixed number (N) of input BufferSource objects
 * and mixes their audio data into a single output AudioBlock. Each input
 * channel can have its gain adjusted individually using fixed-point arithmetic.
 *
 * @tparam N The number of input channels (must be between 2 and 8).
 */
template <size_t N> struct AudioMixer : ::BufferSource {
  static_assert(N >= 2 && N <= 8, "AudioMixer supports 2 to 8 channels");

  /**
   * @brief Constructs an AudioMixer from an existing array of sources.
   * @param initial_sources An array of pointers to the ::BufferSource objects to mix.
   *                        The mixer copies this array but does not take ownership of the pointers.
   *                        Initial gain for all channels is set to 1.0 (256 in Q8.8 format).
   */
  AudioMixer(const etl::array<::BufferSource *, N> &initial_sources) : sources(initial_sources) {
    for (size_t i = 0; i < N; i++) {
      multipliers[i] = 256; // Default gain = 1.0 (Q8.8)
    }
  }

  /**
   * @brief Constructs an AudioMixer from individual source pointers.
   * @tparam SourcePtrs The types of the source pointers (must be convertible to ::BufferSource*).
   * @param source_ptrs A parameter pack of pointers to the ::BufferSource objects to mix.
   *                    The number of arguments must exactly match the template parameter N.
   *                    The mixer copies these pointers but does not take ownership.
   *                    Initial gain for all channels is set to 1.0 (256 in Q8.8 format).
   */
  template <typename... SourcePtrs>
  AudioMixer(SourcePtrs... source_ptrs) : sources{source_ptrs...} {
    static_assert(sizeof...(SourcePtrs) == N,
                  "Number of sources provided must match template parameter N");
    // Ensure all provided types are convertible to ::BufferSource* (compiler implicitly checks this
    // during array initialization)
    for (size_t i = 0; i < N; i++) {
      multipliers[i] = 256; // Default gain = 1.0 (Q8.8)
    }
  }

  /**
   * @brief Fills the output buffer by mixing the input sources.
   *
   * Iterates through each input source, fetches its audio data into a temporary
   * buffer, applies the channel's gain, and accumulates the result into the
   * output buffer. Uses saturating arithmetic to prevent overflow.
   *
   * @param out_samples The AudioBlock to fill with the mixed audio data.
   */
  void fill_buffer(::AudioBlock &out_samples) override {
    // Declare a temporary buffer locally within the function scope.
    ::AudioBlock temp_buffer;

    // Zero out the output buffer first
    for (size_t sample_index = 0; sample_index < out_samples.size(); ++sample_index) {
      out_samples[sample_index] = 0;
    }

    // Mix in each source
    for (size_t channel = 0; channel < N; ++channel) {
      if (sources[channel] != nullptr) { // Check if source is valid
        sources[channel]->fill_buffer(temp_buffer);
        for (size_t sample_index = 0; sample_index < out_samples.size(); ++sample_index) {
          const int16_t multiplier = multipliers[channel]; // Q8.8 format

          // Multiply sample by gain (multiplier is Q8.8, sample is Q1.15)
          // Result is Q9.23. Shift right by 8 to get Q9.15
          // Add to the existing output sample (Q1.15)
          // Saturate the final result back to Q1.15 (int16_t)
          const int32_t value =
              out_samples[sample_index] +
              ((static_cast<int32_t>(temp_buffer[sample_index]) * multiplier) >> 8);
          // Use the 16-bit saturation function from dspinst.h
          out_samples[sample_index] = signed_saturate_rshift16(value, 0);
        }
      }
    }
  }

  void gain(unsigned int channel, float gain) {
    if (channel >= N) {
      return;
    }

    // Clamp gain to representable range for int16_t multiplier (Q8.8)
    // Max positive gain slightly less than 128.0 (32767 / 256)
    // Min negative gain is -128.0 (-32768 / 256)
    if (gain > 127.99f) {
      gain = 127.99f;
    } else if (gain < -128.0f) {
      gain = -128.0f;
    }

    // Convert float gain to Q8.8 fixed-point format
    multipliers[channel] = static_cast<int16_t>(gain * 256.0f);
  }

private:
  etl::array<::BufferSource *, N> sources;
  etl::array<int16_t, N> multipliers;
};

} // namespace musin::audio

#endif
