/* Audio Library for Teensy 3.X
 * Copyright (c) 2016, Byron Jacquot, SparkFun Electronics
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

#ifndef MUSIN_AUDIO_SYNTH_SIMPLE_DRUM_H_
#define MUSIN_AUDIO_SYNTH_SIMPLE_DRUM_H_

#include "audio_output.h"
#include "buffer_source.h"
#include <cstdint>

namespace musin::audio {

// The Teensy original has an (unused in the DUO) frequency-modulation input;
// this port is a pure generator.
struct SynthSimpleDrum : ::BufferSource {
  SynthSimpleDrum() {
    length(600);
    frequency(60);
    pitchMod(0x200);
    wav_amplitude1 = 0x7fff;
    wav_amplitude2 = 0;
  }
  void noteOn();

  void frequency(float freq) {
    const float sample_rate = static_cast<float>(AudioOutput::SAMPLE_FREQUENCY);
    if (freq < 0.0f)
      freq = 0;
    else if (freq > (sample_rate / 2.0f))
      freq = sample_rate / 2.0f;

    wav_increment = (freq * (0x7fffffffLL / sample_rate)) + 0.5f;
  }

  void length(int32_t milliseconds) {
    if (milliseconds < 0)
      return;
    if (milliseconds > 5000)
      milliseconds = 5000;

    int32_t len_samples =
        milliseconds * (AudioOutput::SAMPLE_FREQUENCY / 1000.0f);

    env_decrement = (0x7fff0000 / len_samples);
  };

  void secondMix(float level);
  void pitchMod(float depth);

  void fill_buffer(::AudioBlock &out_samples) override;

private:
  // Envelope params
  int32_t env_lin_current = 0; // present value of linear slope.
  int32_t env_decrement;       // how each sample deviates from previous.

  // Waveform params
  uint32_t wav_phasor = 0;
  uint32_t wav_phasor2 = 0;

  int16_t wav_amplitude1;
  int16_t wav_amplitude2;

  uint32_t wav_increment;
  int32_t wav_pitch_mod;
};

} // namespace musin::audio

#endif /* MUSIN_AUDIO_SYNTH_SIMPLE_DRUM_H_ */
