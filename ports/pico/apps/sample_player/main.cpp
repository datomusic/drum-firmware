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
    samples[i * 2 + 0] = 0; // L
    samples[i * 2 + 1] = 0; // R
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

  AudioOutput::init(fill_buffer, SAMPLES_PER_BUFFER);

  while (true) {
    int c = getchar_timeout_us(0);
    if (c >= 0) {
      if (c == '-' && vol)
        vol--;
      if ((c == '=' || c == '+') && vol < 256)
        vol++;
      if (c == 'q') {
        AudioOutput::deinit();
        break;
      }

      printf("vol = %d      \r", vol);
    }
  }
  puts("\n");
  return 0;
}
