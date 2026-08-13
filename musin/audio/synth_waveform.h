/* Audio Library for Teensy 3.X
 * Copyright (c) 2018, Paul Stoffregen, paul@pjrc.com
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

#ifndef MUSIN_AUDIO_SYNTH_WAVEFORM_H_
#define MUSIN_AUDIO_SYNTH_WAVEFORM_H_

#include "audio_output.h"
#include "buffer_source.h"
#include <cstdint>

// data_waveforms.c
extern "C" {
extern const int16_t AudioWaveformSine[257];
}

#define WAVEFORM_SINE 0
#define WAVEFORM_SAWTOOTH 1
#define WAVEFORM_SQUARE 2
#define WAVEFORM_TRIANGLE 3
#define WAVEFORM_ARBITRARY 4
#define WAVEFORM_PULSE 5
#define WAVEFORM_SAWTOOTH_REVERSE 6
#define WAVEFORM_SAMPLE_HOLD 7
#define WAVEFORM_TRIANGLE_VARIABLE 8
#define WAVEFORM_BANDLIMIT_SAWTOOTH 9
#define WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE 10
#define WAVEFORM_BANDLIMIT_SQUARE 11
#define WAVEFORM_BANDLIMIT_PULSE 12

namespace musin::audio {

class BandLimitedWaveform {
public:
  BandLimitedWaveform();
  int16_t generate_sawtooth(uint32_t new_phase, int i);
  int16_t generate_square(uint32_t new_phase, int i);
  int16_t generate_pulse(uint32_t new_phase, uint32_t pulse_width, int i);
  void init_sawtooth(uint32_t freq_word);
  void init_square(uint32_t freq_word);
  void init_pulse(uint32_t freq_word, uint32_t pulse_width);

private:
  struct step_state {
    int offset;
    bool positive;
  };

  int32_t lookup(int offset);
  void insert_step(int offset, bool rising, int i);
  int32_t process_step(int i);
  int32_t process_active_steps(uint32_t new_phase);
  int32_t process_active_steps_saw(uint32_t new_phase);
  int32_t process_active_steps_pulse(uint32_t new_phase, uint32_t pulse_width);
  void new_step_check_square(uint32_t new_phase, int i);
  void new_step_check_pulse(uint32_t new_phase, uint32_t pulse_width, int i);
  void new_step_check_saw(uint32_t new_phase, int i);

  uint32_t phase_word;
  int32_t dc_offset;
  step_state states[32]; // circular buffer of active steps
  int newptr; // buffer pointers into states, AND'd with PTRMASK to keep in
              // buffer range.
  int delptr;
  int32_t cyclic[16]; // circular buffer of output samples
  bool pulse_state;
  uint32_t sampled_width; // pulse width is sampled once per waveform
};

struct SynthWaveform : ::BufferSource {
  SynthWaveform()
      : phase_accumulator(0), phase_increment(0), phase_offset(0), magnitude(0),
        pulse_width(0x40000000), arbdata(nullptr), sample(0),
        tone_type(WAVEFORM_SINE), tone_offset(0) {
  }

  void frequency(float freq) {
    const float sample_rate = static_cast<float>(AudioOutput::SAMPLE_FREQUENCY);
    if (freq < 0.0f) {
      freq = 0.0f;
    } else if (freq > sample_rate / 2.0f) {
      freq = sample_rate / 2.0f;
    }
    phase_increment =
        static_cast<uint32_t>(freq * (4294967296.0f / sample_rate));
    if (phase_increment > 0x7FFE0000u) {
      phase_increment = 0x7FFE0000;
    }
  }
  void phase(float angle) {
    if (angle < 0.0f) {
      angle = 0.0f;
    } else if (angle > 360.0f) {
      angle = angle - 360.0f;
      if (angle >= 360.0f) {
        return;
      }
    }
    phase_offset = static_cast<uint32_t>(angle * (4294967296.0 / 360.0));
  }
  void amplitude(float n) { // 0 to 1.0
    if (n < 0) {
      n = 0;
    } else if (n > 1.0f) {
      n = 1.0f;
    }
    magnitude = static_cast<int32_t>(n * 65536.0f);
  }
  void offset(float n) {
    if (n < -1.0f) {
      n = -1.0f;
    } else if (n > 1.0f) {
      n = 1.0f;
    }
    tone_offset = static_cast<int16_t>(n * 32767.0f);
  }
  void pulseWidth(float n) { // 0.0 to 1.0
    if (n < 0) {
      n = 0;
    } else if (n > 1.0f) {
      n = 1.0f;
    }
    pulse_width = static_cast<uint32_t>(n * 4294967296.0f);
  }
  void begin(short t_type) {
    phase_offset = 0;
    tone_type = t_type;
    if (t_type == WAVEFORM_BANDLIMIT_SQUARE) {
      band_limit_waveform.init_square(phase_increment);
    } else if (t_type == WAVEFORM_BANDLIMIT_PULSE) {
      band_limit_waveform.init_pulse(phase_increment, pulse_width);
    } else if (t_type == WAVEFORM_BANDLIMIT_SAWTOOTH ||
               t_type == WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE) {
      band_limit_waveform.init_sawtooth(phase_increment);
    }
  }
  void begin(float t_amp, float t_freq, short t_type) {
    amplitude(t_amp);
    frequency(t_freq);
    phase_offset = 0;
    begin(t_type);
  }
  void arbitraryWaveform(const int16_t *data, float) {
    arbdata = data;
  }

  void fill_buffer(::AudioBlock &out_samples) override;

private:
  uint32_t phase_accumulator;
  uint32_t phase_increment;
  uint32_t phase_offset;
  int32_t magnitude;
  uint32_t pulse_width;
  const int16_t *arbdata;
  int16_t sample; // for WAVEFORM_SAMPLE_HOLD
  short tone_type;
  int16_t tone_offset;
  BandLimitedWaveform band_limit_waveform;
};

} // namespace musin::audio

#endif /* MUSIN_AUDIO_SYNTH_WAVEFORM_H_ */
