#include "etl/array.h"
#include <cstdint>
#include <stdio.h>

#include "musin/audio/audio_output.h"
/*
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
*/
#include "musin/audio/memory_reader.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"

#include "support/all_samples.h"

struct MemorySound {
  constexpr MemorySound(const size_t sample_index)
      : sample_index(sample_index),
        reader(all_samples[sample_index].data, all_samples[sample_index].length),
        sound(Sound(reader)) {
  }

  void next_sample() {
    sample_index = (sample_index + 4) % all_samples.size();
    reader.set_source(all_samples[sample_index].data, all_samples[sample_index].length);
  }

  size_t sample_index;
  MemorySampleReader reader;
  Sound sound;
};

static const int SOUND_COUNT = 4;
MemorySound sounds[SOUND_COUNT] = {MemorySound(0), MemorySound(1), MemorySound(2), MemorySound(3)};
const etl::array<BufferSource *, 4> sources = {&sounds[0].sound, &sounds[1].sound, &sounds[2].sound,
                                               &sounds[3].sound};

AudioMixer mixer(sources);
/*
Crusher crusher(mixer);
Lowpass lowpass(crusher);
*/

BufferSource &output_source = mixer;

int main() {
  stdio_init_all();
  sleep_ms(1000);
  printf("Startup!\n");

  AudioOutput::init();

  uint32_t last_ms = to_ms_since_boot(get_absolute_time());
  uint32_t accum_ms = last_ms;

  auto sound_index = 0;
  auto freq_index = 0;
  auto crush_index = 0;

  const std::array freqs{200.0f, 500.0f, 700.0f, 1200.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
  const std::array crush_rates{2489.0f, 44100.0f}; // Also make 44100 a float
  float pitch = 0.3;

  // lowpass.filter.resonance(3.0f);

  AudioOutput::volume(0.7);
  printf("Entering main loop\n");
  while (true) {
    const auto now = to_ms_since_boot(get_absolute_time());
    const auto diff_ms = now - last_ms;
    last_ms = now;
    accum_ms += diff_ms;

    mixer.gain(0, 0.9);
    mixer.gain(1, 0.8);
    mixer.gain(2, 0.85);
    mixer.gain(3, 0.7);

    if (!AudioOutput::update(output_source)) {
      if (accum_ms > 300) {
        accum_ms = 0;
   
        MemorySound &sound = sounds[sound_index];
        if (!sound.reader.has_data()) {
          sound.next_sample();
          sound.sound.play(pitch);
        }

        sound_index = (sound_index + 1) % SOUND_COUNT;

        if (sound_index == 0) {
          pitch += 0.1f;
          if (pitch > 2) {
            pitch = 0.3;
          }

          freq_index = (freq_index + 1) % freqs.size();
          crush_index = (crush_index + 1) % crush_rates.size();

          const auto freq = freqs[freq_index];
          const auto crush = crush_rates[crush_index];

          printf("freq: %f, crush: %f\n", freq, crush);
          // lowpass.filter.frequency(freq);
          // crusher.sampleRate(crush);
        }
      }
    }
  }

  return 0;
}
