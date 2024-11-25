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

// #include "timestretched/AudioSampleKick.h"
#include "timestretched/AudioSampleSnare.h"
#include "timestretched/audio_memory_reader.h"
#include "timestretched/pitch_shifter.h"

#define SINE_WAVE_TABLE_LEN 2048
static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];

static void fill_sine_table() {
  for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
    sine_wave_table[i] =
        32767 * cosf(i * 2 * (float)(M_PI / SINE_WAVE_TABLE_LEN));
  }
}

const uint32_t step0 = 0x060000;
const uint32_t step1 = 0x040000;
uint32_t pos0 = 0;
uint32_t pos1 = 0;
const uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
uint volume = 10;

static void sine_fill_buffer(audio_buffer_t *buffer) {
  int32_t *samples = (int32_t *)buffer->buffer->bytes;
  for (uint i = 0; i < buffer->max_sample_count; i++) {
    int32_t value0 = (volume * sine_wave_table[pos0 >> 16u]) << 8u;
    int32_t value1 = (volume * sine_wave_table[pos1 >> 16u]) << 8u;
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
}
double playback_speed = 1;

struct Sound {
  Sound(const unsigned int *sample_data, const size_t data_length)
      : memory_reader(sample_data, data_length), pitch_shifter(memory_reader) {
  }

  void play() {
    printf("Playing drum\n");
    pitch_shifter.set_speed(playback_speed);
    pitch_shifter.reset();
  }

  void fill_buffer(audio_buffer_t *out_buffer) {
    int32_t *out_samples = (int32_t *)out_buffer->buffer->bytes;

    // printf("Max samples: %i\n", out_buffer->max_sample_count);
    if (pitch_shifter.has_data()) {
      int16_t source_buffer[AUDIO_BLOCK_SAMPLES];
      const uint32_t read_count = pitch_shifter.read_samples(source_buffer);
      printf("Read %i samples, max: %i\n", read_count,
             out_buffer->max_sample_count);
      for (uint i = 0; i < read_count; i++) {
        int32_t sample = (volume * source_buffer[i]) << 8u;
        // use 32bit full scale
        sample = sample + (sample >> 16u);
        out_samples[i * 2 + 0] = sample; // L
        out_samples[i * 2 + 1] = sample; // R
      }
      out_buffer->sample_count = read_count;
    } else {
      printf("Filling empty buffer\n");
      for (uint i = 0; i < out_buffer->max_sample_count; i++) {
        out_samples[i * 2 + 0] = 0; // L
        out_samples[i * 2 + 1] = 0; // R
      }
      out_buffer->sample_count = out_buffer->max_sample_count;
    }
  }

  AudioMemoryReader memory_reader;
  PitchShifter pitch_shifter;
};

// Sound sound(AudioSampleKick, AudioSampleKickSize);
Sound sound(AudioSampleSnare, AudioSampleSnareSize);

static const uint32_t PIN_DCDC_PSM_CTRL = 23;

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

  sound.fill_buffer(buffer);
  // sine_fill_buffer(buffer);
  give_audio_buffer(pool, buffer);
}

static bool interactive_ui() {
  int c = getchar_timeout_us(0);
  if (c >= 0) {
    if (c == '-' && volume)
      volume--;
    if ((c == '=' || c == '+') && volume < 256)
      volume++;
    if (c == 'q') {
      AudioOutput::deinit();
      return false;
    }

    if (c == 'p') {
      sound.play();
    }

    printf("volume = %d      \r", volume);
  }

  return true;
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

  fill_sine_table();
  AudioOutput::init(fill_buffer, AUDIO_BLOCK_SAMPLES);

  while (true) {
    /*
    if (!interactive_ui()) {
      break;
    }
    */
    sleep_ms(1000);
    sound.play();
  }
  puts("\n");
  return 0;
}
