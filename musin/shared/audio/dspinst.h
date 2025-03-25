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

#include <stdint.h>

// computes limit((val >> rshift), 2**bits)
static inline int32_t signed_saturate_rshift(int32_t val, int32_t bits,
                                             int32_t rshift)
    __attribute__((always_inline, unused));
static inline int32_t signed_saturate_rshift(int32_t val, int32_t bits,
                                             int32_t rshift) {
  int32_t out, max;
  out = val >> rshift;
  max = 1 << (bits - 1);
  if (out >= 0) {
    if (out > max - 1)
      out = max - 1;
  } else {
    if (out < -max)
      out = -max;
  }
  return out;
}

// computes limit(val, 2**bits)
static inline int16_t saturate16(int32_t val)
    __attribute__((always_inline, unused));
static inline int16_t saturate16(int32_t val) {

  if (val > 32767)
    val = 32767;
  else if (val < -32768)
    val = -32768;
  return val;
}

// computes ((a[31:0] * b[15:0]) >> 16)
static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b)
    __attribute__((always_inline, unused));
static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b) {

  return ((int64_t)a * (int16_t)(b & 0xFFFF)) >> 16;
}

// computes ((a[31:0] * b[31:16]) >> 16)
static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b)
    __attribute__((always_inline, unused));
static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b) {

  return ((int64_t)a * (int16_t)(b >> 16)) >> 16;
}

// computes (((int64_t)a[31:0] * (int64_t)b[31:0]) >> 32)
static inline int32_t multiply_32x32_rshift32(int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline int32_t multiply_32x32_rshift32(int32_t a, int32_t b) {

  return ((int64_t)a * (int64_t)b) >> 32;
}

// computes (((int64_t)a[31:0] * (int64_t)b[31:0] + 0x8000000) >> 32)
static inline int32_t multiply_32x32_rshift32_rounded(int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline int32_t multiply_32x32_rshift32_rounded(int32_t a, int32_t b) {

  return (((int64_t)a * (int64_t)b) + 0x8000000) >> 32;
}

// computes sum + (((int64_t)a[31:0] * (int64_t)b[31:0] + 0x8000000) >> 32)
static inline int32_t
multiply_accumulate_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline int32_t
multiply_accumulate_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b) {

  return sum + ((((int64_t)a * (int64_t)b) + 0x8000000) >> 32);
}

// computes sum - (((int64_t)a[31:0] * (int64_t)b[31:0] + 0x8000000) >> 32)
static inline int32_t
multiply_subtract_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline int32_t
multiply_subtract_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b) {

  return sum - ((((int64_t)a * (int64_t)b) + 0x8000000) >> 32);
}

// computes (a[31:16] | (b[31:16] >> 16))
static inline uint32_t pack_16t_16t(int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline uint32_t pack_16t_16t(int32_t a, int32_t b) {

  return (a & 0xFFFF0000) | ((uint32_t)b >> 16);
}

// computes (a[31:16] | b[15:0])
static inline uint32_t pack_16t_16b(int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline uint32_t pack_16t_16b(int32_t a, int32_t b) {

  return (a & 0xFFFF0000) | (b & 0x0000FFFF);
}

// computes ((a[15:0] << 16) | b[15:0])
static inline uint32_t pack_16b_16b(int32_t a, int32_t b)
    __attribute__((always_inline, unused));
static inline uint32_t pack_16b_16b(int32_t a, int32_t b) {

  return (a << 16) | (b & 0x0000FFFF);
}

// get Q from PSR
static inline uint32_t get_q_psr(void) __attribute__((always_inline, unused));
static inline uint32_t get_q_psr(void) {
  uint32_t out;
  asm("mrs %0, APSR" : "=r"(out));
  return (out & 0x8000000) >> 27;
}

// clear Q BIT in PSR
static inline void clr_q_psr(void) __attribute__((always_inline, unused));
static inline void clr_q_psr(void) {
  uint32_t t;
  asm("mov %[t],#0\n"
      "msr APSR_nzcvq,%0\n"
      : [t] "=&r"(t)::"cc");
}
