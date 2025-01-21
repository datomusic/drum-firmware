/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// #include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/structs/clocks.h"
#include "sine_source.h"

#include "audio_output.h"
#include "pico/audio.h"
#include "pico/stdlib.h"
#include "sound.h"

#include "teensy_audio/mixer.h"
#include "timestretched/AudioSampleCashregister.h"
#include "timestretched/AudioSampleGong.h"
#include "timestretched/AudioSampleHihat.h"
#include "timestretched/AudioSampleKick.h"
#include "timestretched/AudioSampleSnare.h"

#include <vector>

uint master_volume = 10;
Sound kick(AudioSampleKick, AudioSampleKickSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound gong(AudioSampleGong, AudioSampleGongSize);
Sound cashreg(AudioSampleCashregister, AudioSampleCashregisterSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);
// const int SOUND_COUNT = 3;
// BufferSource *sounds[SOUND_COUNT] = {&kick, &snare, &cashreg, &hihat};
// BufferSource *sounds[SOUND_COUNT] = {&kick, &snare, &hihat};
const int SOUND_COUNT = 4;
BufferSource *sounds[SOUND_COUNT] = {&kick, &snare, &hihat, &cashreg};

AudioMixer4 mixer(sounds, SOUND_COUNT);

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

static void __not_in_flash_func(fill_audio_buffer)(audio_buffer_pool_t *pool) {
  audio_buffer_t *out_buffer = take_audio_buffer(pool, false);
  if (out_buffer == NULL) {
    // printf("Failed to take audio buffer\n");
    return;
  }

  static int16_t temp_samples[AUDIO_BLOCK_SAMPLES];
  mixer.fill_buffer(temp_samples);

  // Convert to 32bit stereo
  int32_t *stereo_out_samples = (int32_t *)out_buffer->buffer->bytes;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    int32_t sample = (master_volume * temp_samples[i]) << 8u;
    sample = sample + (sample >> 16u);
    stereo_out_samples[i * 2] = sample;
    stereo_out_samples[i * 2 + 1] = sample;
  }

  out_buffer->sample_count = AUDIO_BLOCK_SAMPLES;
  give_audio_buffer(pool, out_buffer);
}

/*
static bool interactive_ui() {
  int c = getchar_timeout_us(0);
  if (c >= 0) {
    if (c == '-' && sound.volume)
      sound.volume--;
    if ((c == '=' || c == '+') && sound.volume < 256)
      sound.volume++;
    if (c == 'q') {
      AudioOutput::deinit();
      return false;
    }

    if (c == 'p') {
      sound.play();
    }

    printf("volume = %d      \r", sound.volume);
  }

  return true;
}
*/

int main() {
  init_clock();
  stdio_init_all();

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1); // PWM mode for less Audio noise

  // fill_sine_table();
  AudioOutput::init(fill_audio_buffer);

  while (true) {
    // if (!interactive_ui()) { break; }
    mixer.gain(0, 0.9);
    mixer.gain(1, 0.8);
    mixer.gain(2, 0.3);
    mixer.gain(3, 0.7);

    sleep_ms(200);
    kick.play(0.8);
    sleep_ms(200);
    cashreg.play(0.8);
    hihat.play(0.4);
    sleep_ms(200);
    kick.play(1.8);
    sleep_ms(200);
    hihat.play(0.8);
    sleep_ms(200);
    kick.play(0.8);
    sleep_ms(200);
    hihat.play(1.2);
    sleep_ms(200);
    kick.play(1.8);
    sleep_ms(200);
    hihat.play(1.7);
    kick.play(0.9);
    snare.play(1.5);
  }

  return 0;
}
