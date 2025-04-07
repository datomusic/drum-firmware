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

/**
 * @file waveshaper.h
 * @brief Defines a waveshaper audio effect based on lookup table interpolation.
 * Adapted from the Teensy Audio Library effect by Damien Clarke.
 */

#ifndef MUSIN_WAVESHAPER_H_
#define MUSIN_WAVESHAPER_H_

#include "block.h" // For AudioBlock
#include "buffer_source.h"
#include "etl/vector.h" // For etl::vector
#include <cstddef>     // For size_t
#include <cstdint>     // For int16_t, uint16_t
#include <cmath>       // For std::log2, std::pow

/**
 * @brief Applies waveshaping distortion to an audio signal using a lookup table.
 *
 * The shape of the distortion is defined by an array provided via the `shape()` method.
 * The input signal is used as an index into the table, with linear interpolation
 * applied between table points.
 */
struct Waveshaper : BufferSource {
public:
  /**
   * @brief Maximum size of the waveshaper lookup table (number of points).
   * Must be a power of two plus one (e.g., 257, 513, 1025).
   * 1025 points require ~2KB of RAM.
   */
  static constexpr size_t MAX_WAVESHAPE_SIZE = 1025;

  /**
   * @brief Constructs a Waveshaper effect.
   * @param source The BufferSource providing the input audio signal.
   */
  Waveshaper(BufferSource &source) : source(source), lerpshift(0) {}

  // Note: No destructor needed as etl::vector manages its own memory (statically allocated).

  /**
   * @brief Fills the output buffer by applying the waveshaping effect.
   * Fetches audio from the source, then applies the waveshaping based on the
   * current lookup table. If no shape is set, the audio passes through unchanged.
   * @param out_samples The AudioBlock to fill/modify.
   */
  void fill_buffer(AudioBlock &out_samples) override;

  /**
   * @brief Sets the waveshape lookup table.
   *
   * @param new_shape A pointer to an array of floats defining the shape.
   *                  Values should typically be in the range [-1.0, 1.0].
   *                  The values are scaled and copied into an internal int16_t table.
   * @param length The number of points in the `new_shape` array.
   *               Must be greater than 1 and less than or equal to MAX_WAVESHAPE_SIZE.
   *               For optimal interpolation, `length - 1` should be a power of two
   *               (e.g., length = 257, 513, 1025). If not, performance might be slightly reduced.
   */
  void shape(const float *new_shape, size_t length);

private:
  BufferSource &source;
  etl::vector<int16_t, MAX_WAVESHAPE_SIZE> waveshape_table;
  int16_t lerpshift; // Precalculated shift amount for interpolation index
};

#endif // MUSIN_WAVESHAPER_H_
