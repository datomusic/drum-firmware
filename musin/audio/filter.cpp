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

#include "filter.h"
#include "dspinst.h"
#include "port/section_macros.h"

namespace musin::audio {

namespace { // Anonymous namespace for implementation-specific constants and helpers

// --- Fixed-point arithmetic constants ---
// MULT macro replacement: (multiply_32x32_rshift32_rounded(a, b) << 2)
// This is effectively a right shift by 30 with rounding from the rshift32 part.
static inline int32_t multiply_32x32_rshift30_rounded(int32_t a, int32_t b) {
  return (multiply_32x32_rshift32_rounded(a, b) << 2);
}

// Input scaling for filter processing
constexpr int32_t INPUT_SCALE_LSHIFT = 12;
// Right shift for averaging input in Chamberlin filter ( (input + inputprev) >> 1 )
constexpr int32_t INPUT_AVG_RSHIFT = 1;
// Output scaling for final sample value
constexpr int32_t OUTPUT_SCALE_RSHIFT =
    13; // Results in Q1.15 if input is Q1.15 and intermediate is Q.27

// --- Constants for update_variable (frequency modulation) ---
// Mask to get fractional part of control signal for exp2 approximation
constexpr int32_t N_CONTROL_FRAC_MASK = 0x7FFFFFF; // 27 fractional bits

#ifdef IMPROVE_EXPONENTIAL_ACCURACY
// exp2 polynomial by Stefan Stenzel: x = n << 3
constexpr int32_t X_N_LSHIFT_PRE_POLY_STEFAN = 3;
constexpr int32_t EXP2_POLY_STEFAN_C0 = 536870912;         // Q29
constexpr int32_t EXP2_POLY_STEFAN_C1 = 1494202713;        // Q29
constexpr int32_t EXP2_POLY_STEFAN_C2 = 1934101615;        // Q29
constexpr int32_t EXP2_POLY_STEFAN_C3_FACTOR = 1358044250; // Q29
// Post-accumulation shifts for Stefan Stenzel polynomial terms
constexpr int32_t EXP2_POLY_STEFAN_C2_POST_LSHIFT = 1;
constexpr int32_t EXP2_POLY_STEFAN_C3_POST_LSHIFT = 1;
#else
// exp2 algorithm by Laurent de Soras: (n + C0) << C1_SHIFT
constexpr int32_t N_LAURENT_OFFSET = 134217728; // Q27
constexpr int32_t N_LAURENT_LSHIFT = 3;
constexpr int32_t EXP2_LAURENT_C0 = 715827883; // Q29
constexpr int32_t EXP2_LAURENT_C1 = 715827882; // Q29
#endif

// Final scaling for 'n' based on integer part of control signal
constexpr int32_t N_FINAL_RSHIFT_BASE = 6;
constexpr int32_t CONTROL_INT_RSHIFT = 27; // To get integer part of control

// fmult limits and scaling
constexpr int32_t FMULT_MAX_VAL = 5378279; // Max value for fmult before final shift
constexpr int32_t FMULT_POST_SCALE_LSHIFT = 8;

#ifdef IMPROVE_HIGH_FREQUENCY_ACCURACY
// Polynomial approximation for high frequency accuracy
constexpr int32_t HIGH_FREQ_ACC_POLY_C0 = 2145892402;         // Q31
constexpr int32_t HIGH_FREQ_ACC_POLY_C1_FACTOR = -1383276101; // Q31
constexpr int32_t HIGH_FREQ_ACC_POST_LSHIFT = 1;
#endif

} // namespace

// State Variable Filter (Chamberlin) with 2X oversampling
// http://www.musicdsp.org/showArchiveComment.php?ArchiveID=92

// It's very unlikely anyone could hear any difference, but if you
// care deeply about numerical precision in seldom-used cases,
// uncomment this to improve the control signal accuracy
// #define IMPROVE_HIGH_FREQUENCY_ACCURACY

// This increases the exponential approximation accuracy from
// about 0.341% error to only 0.012% error, which probably makes
// no audible difference.
// #define IMPROVE_EXPONENTIAL_ACCURACY

void __time_critical_func(Filter::update_fixed)(const AudioBlock &in, Filter::Outputs &outputs) {
  const int16_t *end = in.cend();
  const int16_t *input_iterator = in.cbegin();

  int32_t input, inputprev;
  int32_t lowpass, bandpass, highpass;
  int32_t lowpasstmp, bandpasstmp, highpasstmp;
  int32_t fmult, damp;

  int16_t *lowpass_iterator = outputs.lowpass.begin();
  int16_t *bandpass_iterator = outputs.bandpass.begin();
  int16_t *highpass_iterator = outputs.highpass.begin();

  fmult = setting_fmult;
  damp = setting_damp;
  inputprev = state_inputprev;
  lowpass = state_lowpass;
  bandpass = state_bandpass;
  do {
    input = (*input_iterator++) << INPUT_SCALE_LSHIFT;
    lowpass = lowpass + multiply_32x32_rshift30_rounded(fmult, bandpass);
    highpass = ((input + inputprev) >> INPUT_AVG_RSHIFT) - lowpass -
               multiply_32x32_rshift30_rounded(damp, bandpass);
    inputprev = input;
    bandpass = bandpass + multiply_32x32_rshift30_rounded(fmult, highpass);
    lowpasstmp = lowpass;
    bandpasstmp = bandpass;
    highpasstmp = highpass;
    lowpass = lowpass + multiply_32x32_rshift30_rounded(fmult, bandpass);
    highpass = input - lowpass - multiply_32x32_rshift30_rounded(damp, bandpass);
    bandpass = bandpass + multiply_32x32_rshift30_rounded(fmult, highpass);
    lowpasstmp = signed_saturate_rshift16(lowpass + lowpasstmp, OUTPUT_SCALE_RSHIFT);
    bandpasstmp = signed_saturate_rshift16(bandpass + bandpasstmp, OUTPUT_SCALE_RSHIFT);
    highpasstmp = signed_saturate_rshift16(highpass + highpasstmp, OUTPUT_SCALE_RSHIFT);
    *lowpass_iterator++ = lowpasstmp;
    *bandpass_iterator++ = bandpasstmp;
    *highpass_iterator++ = highpasstmp;
  } while (input_iterator < end);

  state_inputprev = inputprev;
  state_lowpass = lowpass;
  state_bandpass = bandpass;
}

void __time_critical_func(Filter::update_variable)(const AudioBlock &in, const AudioBlock &ctl,
                                                   Filter::Outputs &outputs) {
  const int16_t *end = in.cend();
  const int16_t *input_iterator = in.cbegin();
  const int16_t *ctl_iterator = ctl.cbegin();

  int16_t *lowpass_iterator = outputs.lowpass.begin();
  int16_t *bandpass_iterator = outputs.bandpass.begin();
  int16_t *highpass_iterator = outputs.highpass.begin();

  int32_t input, inputprev, control;
  int32_t lowpass, bandpass, highpass;
  int32_t lowpasstmp, bandpasstmp, highpasstmp;
  int32_t fcenter, fmult, damp, octavemult;
  int32_t n;

  fcenter = setting_fcenter;
  octavemult = setting_octavemult;
  damp = setting_damp;
  inputprev = state_inputprev;
  lowpass = state_lowpass;
  bandpass = state_bandpass;
  do {
    // compute fmult using control input, fcenter and octavemult
    control = *ctl_iterator++; // signal is always 15 fractional bits
    control *= octavemult; // octavemult range: 0 to 28671 (12 frac bits) -> control is now Q15.12
                           // (27 total, 12 frac)
    n = control & N_CONTROL_FRAC_MASK; // 27 fractional control bits (actually Q0.27 from Q15.12)
#ifdef IMPROVE_EXPONENTIAL_ACCURACY
    // exp2 polynomial suggested by Stefan Stenzel on "music-dsp"
    // mail list, Wed, 3 Sep 2014 10:08:55 +0200
    // n is Q0.27. x becomes Q0.30
    int32_t x = n << X_N_LSHIFT_PRE_POLY_STEFAN;
    // All terms are Q29. Result is Q29.
    n = multiply_accumulate_32x32_rshift32_rounded(EXP2_POLY_STEFAN_C0, x, EXP2_POLY_STEFAN_C1);
    // x is Q0.30, sq is Q0.28 ( (Q0.30 * Q0.30) >> 32 )
    int32_t sq = multiply_32x32_rshift32_rounded(x, x);
    // n (Q29), sq (Q0.28), C2 (Q29). Result is Q29.
    n = multiply_accumulate_32x32_rshift32_rounded(n, sq, EXP2_POLY_STEFAN_C2);
    // sq(Q0.28), x(Q0.30), C3_FACTOR(Q29). (x*C3_FACTOR)>>32 is Q0.27. (sq * Q0.27)>>32 is Q(-5).
    // Shifted by 1 makes it Q(-4) This part seems to have precision issues or complex Q format
    // interactions. Original: (multiply_32x32_rshift32_rounded(sq,
    // multiply_32x32_rshift32_rounded(x, 1358044250)) << 1); Assuming the original intent was to
    // keep things aligned for Q29 result after n = n + term Let's re-evaluate Q formats if issues
    // arise. For now, direct constant replacement.
    int32_t term3_intermediate =
        multiply_32x32_rshift32_rounded(x, EXP2_POLY_STEFAN_C3_FACTOR); // Q0.30 * Q29 -> Q0.27
    term3_intermediate =
        multiply_32x32_rshift32_rounded(sq, term3_intermediate); // Q0.28 * Q0.27 -> Q(-5)
    n = n + (term3_intermediate
             << EXP2_POLY_STEFAN_C3_POST_LSHIFT); // Q29 + Q(-4) -> Q29 (if lower bits truncated)
    n = n << EXP2_POLY_STEFAN_C2_POST_LSHIFT;     // Q29 -> Q30
#else
    // exp2 algorithm by Laurent de Soras
    // https://www.musicdsp.org/en/latest/Other/106-fast-exp2-approximation.html
    // n is Q0.27. (n + Q27_offset) -> Q0.27. Shifted by 3 -> Q0.30
    n = (n + N_LAURENT_OFFSET) << N_LAURENT_LSHIFT;
    // (Q0.30 * Q0.30) >> 32 -> Q0.28
    n = multiply_32x32_rshift32_rounded(n, n);
    // (Q0.28 * Q29) >> 32 -> Q(-5). Shifted by 3 -> Q(-2).
    // n + Q29 -> Q29. Result is Q29.
    n = multiply_32x32_rshift32_rounded(n, EXP2_LAURENT_C0)
        << N_LAURENT_LSHIFT; // Original had <<3 here
    n = n + EXP2_LAURENT_C1; // Q29 + Q29 -> Q29
#endif
    // n is Q29 or Q30. control is Q15.12. (control >> 27) extracts integer part of control (top 4
    // bits of original 15.12 if it was < 16) Shift amount is (6 - int_part_of_control). n is then
    // shifted right. Result is fmult_scaling_factor.
    n = n >> (N_FINAL_RSHIFT_BASE - (control >> CONTROL_INT_RSHIFT));
    fmult = multiply_32x32_rshift32_rounded(fcenter, n); // fcenter (Q.31), n (scaled). fmult is Q.X
    if (fmult > FMULT_MAX_VAL)
      fmult = FMULT_MAX_VAL;
    fmult =
        fmult << FMULT_POST_SCALE_LSHIFT; // fmult becomes Q4.28 (compatible with MULT macro usage)
// fmult is within 0.4% accuracy for all but the top 2 octaves
// of the audio band.  This math improves accuracy above 5 kHz.
// Without this, the filter still works fine for processing
// high frequencies, but the filter's corner frequency response
// can end up about 6% higher than requested.
#ifdef IMPROVE_HIGH_FREQUENCY_ACCURACY
    // From "Fast Polynomial Approximations to Sine and Cosine"
    // Charles K Garrett, http://krisgarrett.net/
    // fmult (Q4.28). C0 (Q31). (fmult*C0)>>32 is Q3.27
    // (fmult*fmult)>>32 is Q8.24. (Q8.24 * fmult)>>32 is Q12.19
    // (Q12.19 * C1_FACTOR)>>32 is Q11.18
    // Summing Q3.27 and Q11.18 requires alignment or careful Q-format tracking.
    // Original code implies direct sum then shift.
    int32_t term0 = multiply_32x32_rshift32_rounded(fmult, HIGH_FREQ_ACC_POLY_C0);
    int32_t fmult_sq = multiply_32x32_rshift32_rounded(fmult, fmult);
    int32_t fmult_cube = multiply_32x32_rshift32_rounded(fmult_sq, fmult);
    int32_t term1 = multiply_32x32_rshift32_rounded(fmult_cube, HIGH_FREQ_ACC_POLY_C1_FACTOR);
    fmult = (term0 + term1) << HIGH_FREQ_ACC_POST_LSHIFT;
#endif
    // now do the state variable filter as normal, using fmult
    input = (*input_iterator++) << INPUT_SCALE_LSHIFT;
    lowpass = lowpass + multiply_32x32_rshift30_rounded(fmult, bandpass);
    highpass = ((input + inputprev) >> INPUT_AVG_RSHIFT) - lowpass -
               multiply_32x32_rshift30_rounded(damp, bandpass);
    inputprev = input;
    bandpass = bandpass + multiply_32x32_rshift30_rounded(fmult, highpass);
    lowpasstmp = lowpass;
    bandpasstmp = bandpass;
    highpasstmp = highpass;
    lowpass = lowpass + multiply_32x32_rshift30_rounded(fmult, bandpass);
    highpass = input - lowpass - multiply_32x32_rshift30_rounded(damp, bandpass);
    bandpass = bandpass + multiply_32x32_rshift30_rounded(fmult, highpass);
    lowpasstmp = signed_saturate_rshift16(lowpass + lowpasstmp, OUTPUT_SCALE_RSHIFT);
    bandpasstmp = signed_saturate_rshift16(bandpass + bandpasstmp, OUTPUT_SCALE_RSHIFT);
    highpasstmp = signed_saturate_rshift16(highpass + highpasstmp, OUTPUT_SCALE_RSHIFT);
    *lowpass_iterator++ = lowpasstmp;
    *bandpass_iterator++ = bandpasstmp;
    *highpass_iterator++ = highpasstmp;
  } while (input_iterator < end);
  state_inputprev = inputprev;
  state_lowpass = lowpass;
  state_bandpass = bandpass;
}

/*
void Filter::update_variable(AudioBlock &input, AudioBlock &control,
                     Filter::Outputs &outputs) {
  filter_variable(input.begin(), control.begin(), outputs.lowpass.begin(),
                  outputs.bandpass.begin(), outputs.highpass.begin());
}
*/
} // namespace musin::audio
