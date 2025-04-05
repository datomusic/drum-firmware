/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// #include <stdio.h>

#define PICO_AUDIO_I2S_DATA_PIN 18
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 16

#include "pico/audio.h"
#include "pico/stdlib.h"

#include "musin/audio/audio_output.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"

#include "samples/AudioSampleCashregister.h"
#include "samples/AudioSampleGong.h"
#include "samples/AudioSampleHihat.h"
#include "samples/AudioSampleKick.h"
#include "samples/AudioSampleSnare.h"
#include "sine_source.h"

#include <stdio.h>

uint master_volume = 10;
Sound kick(AudioSampleKick, AudioSampleKickSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound gong(AudioSampleGong, AudioSampleGongSize);
Sound cashreg(AudioSampleCashregister, AudioSampleCashregisterSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);
const int SOUND_COUNT = 4;

Sound *sounds[SOUND_COUNT] = {&kick, &snare, &hihat, &cashreg};

AudioMixer4 mixer((BufferSource **)sounds, SOUND_COUNT);

static const uint32_t PIN_DCDC_PSM_CTRL = 23;

static void __not_in_flash_func(fill_audio_buffer)(audio_buffer_t *out_buffer) {
  // printf("Filling buffer\n");

  static int16_t temp_samples[AUDIO_BLOCK_SAMPLES];
  mixer.fill_buffer(temp_samples);

  // Convert to 32bit stereo
  int16_t *stereo_out_samples = (int16_t *)out_buffer->buffer->bytes;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    stereo_out_samples[i] = (master_volume * temp_samples[i]) >> 8u;
  }

  out_buffer->sample_count = AUDIO_BLOCK_SAMPLES;
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("Startup!\n");

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1); // PWM mode for less Audio noise

  AudioOutput::init();

  printf("Entering main loop\n");

  uint32_t last_ms = to_ms_since_boot(get_absolute_time());
  uint32_t accum_ms = last_ms;

  auto sound_index = 0;
  auto pitch_index = 0;
  const auto pitch_count = 5;
  const float pitches[pitch_count] = {0.6, 0.3, 1, 1.9, 1.4};

  while (true) {
    const auto now = to_ms_since_boot(get_absolute_time());
    const auto diff_ms = now - last_ms;
    last_ms = now;
    accum_ms += diff_ms;

    if (!AudioOutput::update(fill_audio_buffer)) {
      if (accum_ms > 300) {
        accum_ms = 0;
        printf("Playing sound\n");

        mixer.gain(0, 0.9);
        mixer.gain(1, 0.8);
        mixer.gain(2, 0.3);
        mixer.gain(3, 0.7);

        auto sound = sounds[sound_index];
        pitch_index = (pitch_index + 1) % pitch_count;
        const auto pitch = pitches[pitch_index];
        sound->play(pitch);
        sound_index = (sound_index + 1) % SOUND_COUNT;
      }
    }
  }

  return 0;
}
