/*
 * Waveshaper for Teensy 3.X audio
 *
 * Copyright (c) 2017 Damien Clarke, http://damienclarke.me
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "waveshaper.h"
#include "etl/utility.h" // For etl::max
#include <algorithm>     // For std::min
#include <cstddef>       // For size_t
#include <cstdint>       // For int16_t, uint16_t, int32_t

// Destructor is no longer needed as etl::vector handles memory.

void Waveshaper::shape(const float *new_shape, size_t length) {
  // Validate input shape and length
  if (!new_shape || length < 2 || length > MAX_WAVESHAPE_SIZE) {
    // Consider adding logging or assertion here if needed
    waveshape_table.clear(); // Invalidate current shape on error
    lerpshift = 0;
    return;
  }

  // Clear previous shape and resize internal table
  waveshape_table.resize(length);

  // Copy and scale the shape data from float [-1.0, 1.0] to int16_t
  for (size_t i = 0; i < length; ++i) {
    float val = new_shape[i];
    // Clamp input float to [-1.0, 1.0] before scaling
    val = etl::max(-1.0f, std::min(1.0f, val));
    waveshape_table[i] = static_cast<int16_t>(val * 32767.0f);
  }

  // Calculate lerpshift for interpolation.
  // This determines how many bits to shift the input sample (0-65535)
  // to get the base index into the waveshape table.
  // It's calculated such that the highest index (length - 2) is reachable.
  size_t index_range = length - 1; // Number of segments in the table
  if (index_range == 0) {
    lerpshift = 16; // Avoid issues if length is somehow 1 (should be caught above)
  } else {
    // Find the number of bits needed to represent the index range [0, length-2].
    // This is equivalent to floor(log2(index_range-1)) + 1 if index_range > 1,
    // or simply finding the position of the most significant bit.
    // lerpshift = 16 - (number of bits needed for index)
    size_t bits_needed = 0;
    size_t temp_range = index_range; // Use index_range (length-1) here
    while (temp_range > 0) {
      temp_range >>= 1;
      bits_needed++;
    }
    // If index_range is exactly power of 2 (e.g. 1024 for length 1025), bits_needed is correct.
    // If not (e.g. 1000 for length 1001), we still need the same number of bits as the next power
    // of 2.
    lerpshift = 16 - bits_needed;

    // Original logic check: (length - 1) should be power of two for optimal lerp shift
    // bool is_power_of_two = ((length - 1) > 0) && (((length - 1) & (length - 2)) == 0);
    // if (!is_power_of_two) { /* maybe log warning */ }
  }
}

void Waveshaper::fill_buffer(AudioBlock &out_samples) {
  // First, get the audio data from the upstream source
  source.fill_buffer(out_samples);

  // If no waveshape table is loaded or it's too small, pass audio through unmodified
  if (waveshape_table.size() < 2) {
    return;
  }

  // Apply waveshaping
  for (size_t i = 0; i < out_samples.size(); ++i) {
    // Bring int16_t sample [-32768, 32767] into uint16_t range [0, 65535]
    // This range directly maps to the waveshape table indices
    uint16_t x = static_cast<uint16_t>(out_samples[i] + 32768);

    // Calculate base index (xa) and get corresponding table values (ya, yb)
    // Use lerpshift to scale the 16-bit input range to the table index range
    uint16_t xa = x >> lerpshift;

    // Ensure xa+1 doesn't go out of bounds (can happen with input 65535)
    // The maximum index is waveshape_table.size() - 1.
    // We need xa and xa+1, so the maximum allowed value for xa is size() - 2.
    if (xa >= waveshape_table.size() - 1) {
      xa = waveshape_table.size() - 2; // Clamp index
      // When clamped (input is at max value), output the last point directly.
      out_samples[i] = waveshape_table[xa + 1];
      continue; // Skip interpolation for this sample
    }

    int16_t ya = waveshape_table[xa];
    int16_t yb = waveshape_table[xa + 1];

    // Linear interpolation (lerp) between ya and yb
    // (from http://coranac.com/tonc/text/fixed.htm)
    // fraction = lower bits of x = (x & ((1 << lerpshift) - 1))
    // result = ya + ((yb - ya) * fraction) >> lerpshift;
    // Use 32-bit intermediate for multiplication to avoid overflow
    int32_t fraction = static_cast<int32_t>(x & ((1 << lerpshift) - 1)); // Get the fractional part
    int32_t delta = static_cast<int32_t>(yb - ya);
    out_samples[i] = ya + static_cast<int16_t>((delta * fraction) >> lerpshift);
  }
}
