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

#include "etl/vector.h"
#include "musin/audio/audio_output.h"
#include "musin/audio/buffer_source.h"

struct Crusher : BufferSource {
  Crusher(BufferSource &source) : source(source) {
  }

  uint32_t fill_buffer(int16_t *out_samples) {
    const auto sample_count = source.fill_buffer(out_samples);

    // TODO: Remove this when fill_buffer is adjusted to take a vector instead.
    etl::vector<int16_t, AUDIO_BLOCK_SAMPLES> temp_samples(
        out_samples, out_samples + sample_count);

    crush(temp_samples);

    for (uint32_t i = 0; i < sample_count; ++i) {
      out_samples[i] = temp_samples[i];
    }

    return sample_count;
  }

  void bits(uint8_t b) {
    if (b > 16)
      b = 16;
    else if (b == 0)
      b = 1;
    crushBits = b;
  }

  void sampleRate(const float hz) {
    int n = (AudioOutput::SAMPLE_FREQUENCY / hz) + 0.5f;
    if (n < 1)
      n = 1;
    else if (n > 64)
      n = 64;
    sampleStep = n;
  }

private:
  void crush(etl::vector<int16_t, AUDIO_BLOCK_SAMPLES> &samples) {
    uint32_t i;
    uint32_t sampleSquidge;
    uint32_t sampleSqueeze; // squidge is bitdepth, squeeze is for samplerate

    if (sampleStep <= 1) { // no sample rate mods, just crush the bitdepth.
      for (i = 0; i < samples.size(); i++) {
        // shift bits right to cut off fine detail sampleSquidge is a
        // uint32 so sign extension will not occur, fills with zeroes.
        sampleSquidge = samples[i] >> (16 - crushBits);

        // shift bits left again to regain the volume level.
        // fills with zeroes.
        samples[i] = sampleSquidge << (16 - crushBits);
      }
    } else if (crushBits == 16) {
      // bitcrusher not being used, samplerate mods only.
      i = 0;
      while (i < samples.size()) {
        // save the root sample. this will pick up a root
        // sample every _sampleStep_ samples.
        sampleSqueeze = samples[i]; // block->data[i];
        for (int j = 0; j < sampleStep && i < samples.size(); j++) {
          // for each repeated sample, paste in the current
          // root sample, then move onto the next step.
          /* block->data[i] */ samples[i] = sampleSqueeze;
          i++;
        }
      }
    } else { // both being used. crush those bits and mash those samples.
      i = 0;
      while (i < samples.size()) {
        // save the root sample. this will pick up a root sample
        // every _sampleStep_ samples.
        sampleSqueeze = samples[i]; // block->data[i];
                                    //
        for (int j = 0; j < sampleStep && i < samples.size(); j++) {
          // shift bits right to cut off fine detail sampleSquidge
          // is a uint32 so sign extension will not occur, fills
          // with zeroes.
          sampleSquidge = sampleSqueeze >> (16 - crushBits);

          // shift bits left again to regain the volume level.
          // fills with zeroes. paste into buffer sample +
          // sampleStep offset.
          samples[i] = sampleSquidge << (16 - crushBits);
          i++;
        }
      }
    }
  }

  BufferSource &source;
  uint8_t crushBits = 16; // 16 = off
  uint8_t sampleStep = 1; // the number of samples to double up. This simple
                          // technique only allows a few stepped positions.
};

#endif /* end of include guard: CRUSHER_H_4BACOXIO */
