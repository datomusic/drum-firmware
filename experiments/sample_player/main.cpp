#include "etl/array.h"
#include <cstdint>
#include <stdio.h>

#include "musin/audio/audio_output.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
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

const etl::array<BufferSource *, 4> sounds = {&kick, &snare, &hihat, &cashreg};
AudioMixer<4> mixer(sounds);

Crusher crusher(mixer);

// Lowpass lowpass(mixer);
Lowpass lowpass(crusher);

// static BufferSource& output = mixer;
// BufferSource &master_source = crusher;
BufferSource &master_source = lowpass;

int main() {
  stdio_init_all();
  sleep_ms(1000);
  printf("Startup!\n");

  AudioOutput::init();

  uint32_t last_ms = to_ms_since_boot(get_absolute_time());
  uint32_t accum_ms = last_ms;

  auto sound_index = 0;
  auto pitch_index = 0;
  auto freq_index = 0;
  auto crush_index = 0;

  const std::array pitches{0.6, 0.3, 1, 1.9, 1.4};
  const std::array freqs{200.0f,  500.0f,  700.0f,   1200.0f,
                         2000.0f, 5000.0f, 10000.0f, 20000.0f};
  const std::array crush_rates{2489.0f, 44100};

  lowpass.filter.resonance(3.0f);

  printf("Entering main loop\n");
  while (true) {
    const auto now = to_ms_since_boot(get_absolute_time());
    const auto diff_ms = now - last_ms;
    last_ms = now;
    accum_ms += diff_ms;

    if (!AudioOutput::update(master_source, master_volume)) {
      if (accum_ms > 500) {
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

        if (sound_index == 0) {
          freq_index = (freq_index + 1) % freqs.size();
          crush_index = (crush_index + 1) % crush_rates.size();

          const auto freq = freqs[freq_index];
          const auto crush = crush_rates[crush_index];

          printf("freq: %f, crush: %f\n", freq, crush);
          lowpass.filter.frequency(freq);
          crusher.sampleRate(crush);
        }
      }
    }
  }

  return 0;
}
