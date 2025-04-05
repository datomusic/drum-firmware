/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// #include <stdio.h>

#define PICO_AUDIO_I2S_DATA_PIN 18
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 16

#include "etl/array.h"
#include <cstdint>
#include <stdio.h>

#include "pico/audio.h"

#include "musin/audio/audio_output.h"
#include "musin/audio/mixer.h"
#include "musin/audio/teensy/effect_bitcrusher.h"
#include "musin/audio/teensy/filter_variable.h"
#include "musin/audio/sound.h"

#include "samples/AudioSampleCashregister.h"
#include "samples/AudioSampleGong.h"
#include "samples/AudioSampleHihat.h"
#include "samples/AudioSampleKick.h"
#include "samples/AudioSampleSnare.h"

const uint8_t master_volume = 10;
Sound kick(AudioSampleKick, AudioSampleKickSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound gong(AudioSampleGong, AudioSampleGongSize);
Sound cashreg(AudioSampleCashregister, AudioSampleCashregisterSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);

const etl::array<Sound *, 4> sounds = {&kick, &snare, &hihat, &cashreg};
AudioMixer4 mixer({sounds[0], sounds[1], sounds[2], sounds[3]});

AudioEffectBitcrusher crusher;
AudioFilterStateVariable filter;

// BufferSource& output = crusher;
static BufferSource& output = mixer;

static void __not_in_flash_func(fill_audio_buffer)(audio_buffer_t *out_buffer) {
  static int16_t temp_samples[AUDIO_BLOCK_SAMPLES];
  output.fill_buffer(temp_samples);

  int16_t *stereo_out_samples = (int16_t *)out_buffer->buffer->bytes;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    stereo_out_samples[i] = (master_volume * temp_samples[i]) >> 8u;
  }

  out_buffer->sample_count = AUDIO_BLOCK_SAMPLES;
}

int main() {
  stdio_init_all();
  sleep_ms(1000);
  printf("Startup!\n");

  AudioOutput::init();

  uint32_t last_ms = to_ms_since_boot(get_absolute_time());
  uint32_t accum_ms = last_ms;

  auto sound_index = 0;
  auto pitch_index = 0;
  const etl::array pitches{0.6, 0.3, 1, 1.9, 1.4};

  printf("Entering main loop\n");
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

        const auto sound = sounds[sound_index];
        pitch_index = (pitch_index + 1) % pitches.size();
        const auto pitch = pitches[pitch_index];
        sound->play(pitch);
        sound_index = (sound_index + 1) % sounds.size();
      }
    }
  }

  return 0;
}
