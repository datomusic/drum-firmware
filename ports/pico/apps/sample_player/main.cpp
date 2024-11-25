/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/structs/clocks.h"

#include "audio_output.h"
#include "pico/audio.h"
#include "pico/stdlib.h"

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

static const uint32_t PIN_DCDC_PSM_CTRL = 23;

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];
uint32_t step0 = 0x040000;
uint32_t step1 = 0x070000;
uint32_t pos0 = 0;
uint32_t pos1 = 0;
const uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
uint vol = 1;

static void init_clock() {
  // Set PLL_USB 96MHz
  pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
  clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                  96 * MHZ, 48 * MHZ);
  // Change clk_sys to be 96MHz.
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ,
                  96 * MHZ);
  // CLK peri is clocked from clk_sys so need to change clk_peri's freq
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                  96 * MHZ, 96 * MHZ);
}

static void fill_buffer(audio_buffer_pool_t *pool) {
  audio_buffer_t *buffer = take_audio_buffer(pool, false);
  if (buffer == NULL) {
    return;
  }

  int32_t *samples = (int32_t *)buffer->buffer->bytes;
  for (uint i = 0; i < buffer->max_sample_count; i++) {
    int32_t value0 = (vol * sine_wave_table[pos0 >> 16u]) << 8u;
    int32_t value1 = (vol * sine_wave_table[pos1 >> 16u]) << 8u;
    // use 32bit full scale
    samples[i * 2 + 0] = value0 + (value0 >> 16u); // L
    samples[i * 2 + 1] = value1 + (value1 >> 16u); // R

    pos0 += step0;
    pos1 += step1;
    if (pos0 >= pos_max)
      pos0 -= pos_max;
    if (pos1 >= pos_max)
      pos1 -= pos_max;
  }
  buffer->sample_count = buffer->max_sample_count;
  give_audio_buffer(pool, buffer);
}

int main() {
  init_clock();
  stdio_init_all();

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1); // PWM mode for less Audio noise

  for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
    sine_wave_table[i] =
        32767 * cosf(i * 2 * (float)(M_PI / SINE_WAVE_TABLE_LEN));
  }

  AudioOutput::init(fill_buffer, SAMPLES_PER_BUFFER);

  while (true) {
    int c = getchar_timeout_us(0);
    if (c >= 0) {
      if (c == '-' && vol)
        vol--;
      if ((c == '=' || c == '+') && vol < 256)
        vol++;
      if (c == '[' && step0 > 0x10000)
        step0 -= 0x10000;
      if (c == ']' && step0 < (SINE_WAVE_TABLE_LEN / 16) * 0x20000)
        step0 += 0x10000;
      if (c == '{' && step1 > 0x10000)
        step1 -= 0x10000;
      if (c == '}' && step1 < (SINE_WAVE_TABLE_LEN / 16) * 0x20000)
        step1 += 0x10000;
      if (c == 'q') {
        AudioOutput::deinit();
        break;
      }
      printf("vol = %d, step0 = %d, step1 = %d      \r", vol, step0 >> 16,
             step1 >> 16);
    }
  }
  puts("\n");
  return 0;
}
