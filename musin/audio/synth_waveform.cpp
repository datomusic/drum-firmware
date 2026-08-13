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

#include "synth_waveform.h"
#include "dspinst.h"
#include "port/section_macros.h"
#include <cstring>

// uncomment for more accurate but more computationally expensive frequency
// modulation
// #define IMPROVE_EXPONENTIAL_ACCURACY

// 0x7fff won't work due to Gibb's phenomenon, so use 3/4 of full range.
#define BASE_AMPLITUDE 0x6000

namespace {
// Replacement for Arduino random(max) in WAVEFORM_SAMPLE_HOLD; ISR-safe.
uint32_t __time_critical_func(sample_hold_random)(uint32_t max) {
  static uint32_t state = 0x92d68ca2u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return (max == 0) ? 0 : state % max;
}
} // namespace

namespace musin::audio {

void __time_critical_func(SynthWaveform::fill_buffer)(
    ::AudioBlock &out_samples) {
  int16_t *bp, *end;
  int32_t val1, val2;
  int16_t magnitude15;
  uint32_t i, ph, index, index2, scale;
  const uint32_t inc = phase_increment;
  const uint32_t block_samples = out_samples.size();

  ph = phase_accumulator + phase_offset;
  if (magnitude == 0) {
    phase_accumulator += inc * block_samples;
    std::memset(out_samples.begin(), 0, block_samples * sizeof(int16_t));
    return;
  }
  bp = out_samples.begin();

  switch (tone_type) {
  case WAVEFORM_SINE:
    for (i = 0; i < block_samples; i++) {
      index = ph >> 24;
      val1 = AudioWaveformSine[index];
      val2 = AudioWaveformSine[index + 1];
      scale = (ph >> 8) & 0xFFFF;
      val2 *= scale;
      val1 *= 0x10000 - scale;
      *bp++ = multiply_32x32_rshift32(val1 + val2, magnitude);
      ph += inc;
    }
    break;

  case WAVEFORM_ARBITRARY:
    if (!arbdata) {
      phase_accumulator += inc * block_samples;
      std::memset(out_samples.begin(), 0, block_samples * sizeof(int16_t));
      return;
    }
    // len = 256
    for (i = 0; i < block_samples; i++) {
      index = ph >> 24;
      index2 = index + 1;
      if (index2 >= 256)
        index2 = 0;
      val1 = *(arbdata + index);
      val2 = *(arbdata + index2);
      scale = (ph >> 8) & 0xFFFF;
      val2 *= scale;
      val1 *= 0x10000 - scale;
      *bp++ = multiply_32x32_rshift32(val1 + val2, magnitude);
      ph += inc;
    }
    break;

  case WAVEFORM_SQUARE:
    magnitude15 = signed_saturate_rshift(magnitude, 16, 1);
    for (i = 0; i < block_samples; i++) {
      if (ph & 0x80000000) {
        *bp++ = -magnitude15;
      } else {
        *bp++ = magnitude15;
      }
      ph += inc;
    }
    break;

  case WAVEFORM_BANDLIMIT_SQUARE:
    for (i = 0; i < block_samples; i++) {
      uint32_t new_ph = ph + inc;
      int16_t val = band_limit_waveform.generate_square(new_ph, i);
      *bp++ = (val * magnitude) >> 16;
      ph = new_ph;
    }
    break;

  case WAVEFORM_SAWTOOTH:
    for (i = 0; i < block_samples; i++) {
      *bp++ = signed_multiply_32x16t(magnitude, ph);
      ph += inc;
    }
    break;

  case WAVEFORM_SAWTOOTH_REVERSE:
    for (i = 0; i < block_samples; i++) {
      *bp++ = signed_multiply_32x16t(0xFFFFFFFFu - magnitude, ph);
      ph += inc;
    }
    break;

  case WAVEFORM_BANDLIMIT_SAWTOOTH:
  case WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE:
    for (i = 0; i < block_samples; i++) {
      uint32_t new_ph = ph + inc;
      int16_t val = band_limit_waveform.generate_sawtooth(new_ph, i);
      if (tone_type == WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE)
        *bp++ = (val * -magnitude) >> 16;
      else
        *bp++ = (val * magnitude) >> 16;
      ph = new_ph;
    }
    break;

  case WAVEFORM_TRIANGLE:
    for (i = 0; i < block_samples; i++) {
      uint32_t phtop = ph >> 30;
      if (phtop == 1 || phtop == 2) {
        *bp++ = ((0xFFFF - (ph >> 15)) * magnitude) >> 16;
      } else {
        *bp++ = (((int32_t)ph >> 15) * magnitude) >> 16;
      }
      ph += inc;
    }
    break;

  case WAVEFORM_TRIANGLE_VARIABLE:
    do {
      uint32_t rise = 0xFFFFFFFF / (pulse_width >> 16);
      uint32_t fall = 0xFFFFFFFF / (0xFFFF - (pulse_width >> 16));
      for (i = 0; i < block_samples; i++) {
        if (ph < pulse_width / 2) {
          uint32_t n = (ph >> 16) * rise;
          *bp++ = ((n >> 16) * magnitude) >> 16;
        } else if (ph < 0xFFFFFFFF - pulse_width / 2) {
          uint32_t n = 0x7FFFFFFF - (((ph - pulse_width / 2) >> 16) * fall);
          *bp++ = (((int32_t)n >> 16) * magnitude) >> 16;
        } else {
          uint32_t n = ((ph + pulse_width / 2) >> 16) * rise + 0x80000000;
          *bp++ = (((int32_t)n >> 16) * magnitude) >> 16;
        }
        ph += inc;
      }
    } while (0);
    break;

  case WAVEFORM_PULSE:
    magnitude15 = signed_saturate_rshift(magnitude, 16, 1);
    for (i = 0; i < block_samples; i++) {
      if (ph < pulse_width) {
        *bp++ = magnitude15;
      } else {
        *bp++ = -magnitude15;
      }
      ph += inc;
    }
    break;

  case WAVEFORM_BANDLIMIT_PULSE:
    for (i = 0; i < block_samples; i++) {
      int32_t new_ph = ph + inc;
      int32_t val = band_limit_waveform.generate_pulse(new_ph, pulse_width, i);
      *bp++ = (int16_t)((val * magnitude) >> 16);
      ph = new_ph;
    }
    break;

  case WAVEFORM_SAMPLE_HOLD:
    for (i = 0; i < block_samples; i++) {
      *bp++ = sample;
      uint32_t newph = ph + inc;
      if (newph < ph) {
        sample = sample_hold_random(magnitude) - (magnitude >> 1);
      }
      ph = newph;
    }
    break;
  }
  phase_accumulator = ph - phase_offset;

  if (tone_offset) {
    bp = out_samples.begin();
    end = bp + block_samples;
    do {
      val1 = *bp;
      *bp++ = signed_saturate_rshift(val1 + tone_offset, 16, 0);
    } while (bp < end);
  }
}

// BandLimitedWaveform

#define SUPPORT_SHIFT 4
#define SUPPORT (1 << SUPPORT_SHIFT)
#define PTRMASK ((2 << SUPPORT_SHIFT) - 1)

#define SCALE 16
#define SCALE_MASK (SCALE - 1)
#define N (SCALE * SUPPORT * 2)

#define GUARD_BITS 8
#define GUARD (1 << GUARD_BITS)
#define HALF_GUARD (1 << (GUARD_BITS - 1))

#define DEG180 0x80000000u

#define PHASE_SCALE (0x100000000L / (2 * BASE_AMPLITUDE))

} // namespace musin::audio

extern "C" {
extern const int16_t step_table[258];
}

namespace musin::audio {

int32_t __time_critical_func(BandLimitedWaveform::lookup)(int offset) {
  int off = offset >> GUARD_BITS;
  int frac = offset & (GUARD - 1);

  int32_t a, b;
  if (off < N / 2) // handle odd symmetry by reflecting table
  {
    a = step_table[off + 1];
    b = step_table[off + 2];
  } else {
    a = -step_table[N - off];
    b = -step_table[N - off - 1];
  }
  return BASE_AMPLITUDE + ((frac * b + (GUARD - frac) * a + HALF_GUARD) >>
                           GUARD_BITS); // interpolated
}

// create a new step, apply its past waveform into the cyclic sample buffer
// and add a step_state object into active list so it can be added for the
// future samples
void __time_critical_func(BandLimitedWaveform::insert_step)(int offset,
                                                            bool rising,
                                                            int i) {
  while (offset <= (N / 2 - SCALE) << GUARD_BITS) {
    if (offset >= 0)
      cyclic[i & 15] += rising ? lookup(offset) : -lookup(offset);
    offset += SCALE << GUARD_BITS;
    i++;
  }

  states[newptr].offset = offset;
  states[newptr].positive = rising;
  newptr = (newptr + 1) & PTRMASK;
}

// generate value for current sample from one active step, checking for the
// dc_offset adjustment at the end of the table.
int32_t __time_critical_func(BandLimitedWaveform::process_step)(int i) {
  int off = states[i].offset;
  bool positive = states[i].positive;

  int32_t entry = lookup(off);
  off += SCALE << GUARD_BITS;
  states[i].offset = off;     // update offset in table for next sample
  if (off >= N << GUARD_BITS) // at end of step table we alter dc_offset to
                              // extend the step into future
    dc_offset += positive ? 2 * BASE_AMPLITUDE : -2 * BASE_AMPLITUDE;

  return positive ? entry : -entry;
}

// process all active steps for current sample, basically generating the
// waveform portion due only to steps
// square waves use this directly.
int32_t __time_critical_func(BandLimitedWaveform::process_active_steps)(
    uint32_t new_phase) {
  int32_t sample = dc_offset;

  int step_count = (newptr - delptr) & PTRMASK;
  if (step_count > 0) // for any steps in-flight we sum in table entry and
                      // update its state
  {
    int i = newptr;
    do {
      i = (i - 1) & PTRMASK;
      sample += process_step(i);
    } while (i != delptr);
    if (states[delptr].offset >=
        N << GUARD_BITS) // remove any finished entries from the buffer.
    {
      delptr = (delptr + 1) & PTRMASK;
      // can be upto two steps per sample now for pulses
      if (newptr != delptr && states[delptr].offset >= N << GUARD_BITS)
        delptr = (delptr + 1) & PTRMASK;
    }
  }
  return sample;
}

// for sawtooth need to add in the slope and compensate for all the steps being
// one way
int32_t __time_critical_func(BandLimitedWaveform::process_active_steps_saw)(
    uint32_t new_phase) {
  int32_t sample = process_active_steps(new_phase);

  sample += (int16_t)((((uint64_t)phase_word * (2 * BASE_AMPLITUDE)) >> 32) -
                      BASE_AMPLITUDE); // generate the sloped part of the wave

  if (new_phase < DEG180 &&
      phase_word >= DEG180) // detect wrap around, correct dc offset
    dc_offset += 2 * BASE_AMPLITUDE;

  return sample;
}

// for pulse need to adjust the baseline according to the pulse width to cancel
// the DC component.
int32_t __time_critical_func(BandLimitedWaveform::process_active_steps_pulse)(
    uint32_t new_phase, uint32_t pulse_width) {
  int32_t sample = process_active_steps(new_phase);

  return sample + BASE_AMPLITUDE / 2 -
         pulse_width /
             (0x80000000u / BASE_AMPLITUDE); // correct DC offset for duty cycle
}

// Check for new steps using the phase update for the current sample for a
// square wave
void __time_critical_func(BandLimitedWaveform::new_step_check_square)(
    uint32_t new_phase, int i) {
  if (new_phase >= DEG180 && phase_word < DEG180) // detect falling step
  {
    int32_t offset =
        (int32_t)((uint64_t)(SCALE << GUARD_BITS) *
                  (sampled_width - phase_word) / (new_phase - phase_word));
    if (offset == SCALE << GUARD_BITS)
      offset--;
    if (pulse_state) // guard against two falling steps in a row (if pulse width
                     // changing for instance)
    {
      insert_step(-offset, false, i);
      pulse_state = false;
    }
  } else if (new_phase < DEG180 &&
             phase_word >= DEG180) // detect wrap around, rising step
  {
    int32_t offset = (int32_t)((uint64_t)(SCALE << GUARD_BITS) * (-phase_word) /
                               (new_phase - phase_word));
    if (offset == SCALE << GUARD_BITS)
      offset--;
    if (!pulse_state) // guard against two rising steps in a row (if pulse width
                      // changing for instance)
    {
      insert_step(-offset, true, i);
      pulse_state = true;
    }
  }
}

// Checking for new steps for pulse waveform has to deal with changing frequency
// and pulse width and not letting a pulse glitch out of existence as these
// change across a single period of the waveform now we detect the rising edge
// just like for a square wave and use that to sample the pulse width
// parameter, which then has to be checked against the instantaneous frequency
// every sample.
void __time_critical_func(BandLimitedWaveform::new_step_check_pulse)(
    uint32_t new_phase, uint32_t pulse_width, int i) {
  if (pulse_state && phase_word < sampled_width &&
      (new_phase >= sampled_width || new_phase < phase_word)) // falling edge
  {
    int32_t offset =
        (int32_t)((uint64_t)(SCALE << GUARD_BITS) *
                  (sampled_width - phase_word) / (new_phase - phase_word));
    if (offset == SCALE << GUARD_BITS)
      offset--;
    insert_step(-offset, false, i);
    pulse_state = false;
  }
  if ((!pulse_state) && phase_word >= DEG180 &&
      new_phase < DEG180) // detect wrap around, rising step
  {
    // sample the pulse width value so its not changing under our feet later in
    // cycle due to modulation
    sampled_width = pulse_width;

    int32_t offset = (int32_t)((uint64_t)(SCALE << GUARD_BITS) * (-phase_word) /
                               (new_phase - phase_word));
    if (offset == SCALE << GUARD_BITS)
      offset--;
    insert_step(-offset, true, i);
    pulse_state = true;

    if (pulse_state && new_phase >= sampled_width) // detect falling step
                                                   // directly after a rising
                                                   // edge
    {
      int32_t offset =
          (int32_t)((uint64_t)(SCALE << GUARD_BITS) *
                    (sampled_width - phase_word) / (new_phase - phase_word));
      if (offset == SCALE << GUARD_BITS)
        offset--;
      insert_step(-offset, false, i);
      pulse_state = false;
    }
  }
}

// new steps for sawtooth are at 180 degree point, always falling.
void __time_critical_func(BandLimitedWaveform::new_step_check_saw)(
    uint32_t new_phase, int i) {
  if (new_phase >= DEG180 && phase_word < DEG180) // detect falling step
  {
    int32_t offset =
        (int32_t)((uint64_t)(SCALE << GUARD_BITS) * (DEG180 - phase_word) /
                  (new_phase - phase_word));
    if (offset == SCALE << GUARD_BITS)
      offset--;
    insert_step(-offset, false, i);
  }
}

// the generation function pushd new sample into cyclic buffer, having taken out
// the oldest entry to return.  The output is thus 16 samples behind, which
// allows the non-casual step function to work in real time.
int16_t
__time_critical_func(BandLimitedWaveform::generate_sawtooth)(uint32_t new_phase,
                                                             int i) {
  new_step_check_saw(new_phase, i);
  int32_t val = process_active_steps_saw(new_phase);
  int16_t sample = (int16_t)cyclic[i & 15];
  cyclic[i & 15] = val;
  phase_word = new_phase;
  return sample;
}

int16_t
__time_critical_func(BandLimitedWaveform::generate_square)(uint32_t new_phase,
                                                           int i) {
  new_step_check_square(new_phase, i);
  int32_t val = process_active_steps(new_phase);
  int16_t sample = (int16_t)cyclic[i & 15];
  cyclic[i & 15] = val;
  phase_word = new_phase;
  return sample;
}

int16_t __time_critical_func(BandLimitedWaveform::generate_pulse)(
    uint32_t new_phase, uint32_t pulse_width, int i) {
  new_step_check_pulse(new_phase, pulse_width, i);
  int32_t val = process_active_steps_pulse(new_phase, pulse_width);
  int32_t sample = cyclic[i & 15];
  cyclic[i & 15] = val;
  phase_word = new_phase;
  return (int16_t)((sample >> 1) -
                   (sample >> 5)); // scale down to avoid overflow on narrow
                                   // pulses, where the DC shift is big
}

void BandLimitedWaveform::init_sawtooth(uint32_t freq_word) {
  phase_word = 0;
  newptr = 0;
  delptr = 0;
  for (int i = 0; i < 2 * SUPPORT; i++)
    phase_word -= freq_word;
  dc_offset = phase_word < DEG180 ? BASE_AMPLITUDE : -BASE_AMPLITUDE;
  for (int i = 0; i < 2 * SUPPORT; i++) {
    uint32_t new_phase = phase_word + freq_word;
    new_step_check_saw(new_phase, i);
    cyclic[i & 15] = (int16_t)process_active_steps_saw(new_phase);
    phase_word = new_phase;
  }
}

void BandLimitedWaveform::init_square(uint32_t freq_word) {
  init_pulse(freq_word, DEG180);
}

void BandLimitedWaveform::init_pulse(uint32_t freq_word, uint32_t pulse_width) {
  phase_word = 0;
  sampled_width = pulse_width;
  newptr = 0;
  delptr = 0;
  for (int i = 0; i < 2 * SUPPORT; i++)
    phase_word -= freq_word;

  if (phase_word < pulse_width) {
    dc_offset = BASE_AMPLITUDE;
    pulse_state = true;
  } else {
    dc_offset = -BASE_AMPLITUDE;
    pulse_state = false;
  }

  for (int i = 0; i < 2 * SUPPORT; i++) {
    uint32_t new_phase = phase_word + freq_word;
    new_step_check_pulse(new_phase, pulse_width, i);
    cyclic[i & 15] =
        (int16_t)process_active_steps_pulse(new_phase, pulse_width);
    phase_word = new_phase;
  }
}

BandLimitedWaveform::BandLimitedWaveform() {
  newptr = 0;
  delptr = 0;
  dc_offset = BASE_AMPLITUDE;
  phase_word = 0;
}

} // namespace musin::audio
