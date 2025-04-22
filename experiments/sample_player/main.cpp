#include "etl/array.h"
#include <cstdint>
#include <stdio.h>

#include "musin/audio/audio_output.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/audio/pcm_reader.h"
#include "musin/audio/sound.h"

#include "samples.h"

struct SampleData {
  const unsigned char *data;
  const uint32_t length;
};

static const etl::array<SampleData, 32> sample_bank = {
    SampleData{samples_005__pcm, samples_005__pcm_len},
    SampleData{samples_006__pcm, samples_006__pcm_len},
    SampleData{samples_015__pcm, samples_015__pcm_len},
    SampleData{samples_100_nt_snare__pcm, samples_100_nt_snare__pcm_len},
    SampleData{samples_26880__vexst__closed_hi_hat_2_1__pcm,
               samples_26880__vexst__closed_hi_hat_2_1__pcm_len},
    SampleData{samples_26887__vexst__kick_3_1__pcm, samples_26887__vexst__kick_3_1__pcm_len},
    SampleData{samples_26901__vexst__snare_2_1__pcm, samples_26901__vexst__snare_2_1__pcm_len},
    SampleData{samples_44_Analog_Cowbell__pcm, samples_44_Analog_Cowbell__pcm_len},
    SampleData{samples_cabasa__pcm, samples_cabasa__pcm_len},
    SampleData{samples_chihiro_snare__pcm, samples_chihiro_snare__pcm_len},
    SampleData{samples_cowbell_hi__pcm, samples_cowbell_hi__pcm_len},
    SampleData{samples_ClosedHH_909X_2__pcm, samples_ClosedHH_909X_2__pcm_len},
    SampleData{samples_DR110_clap__pcm, samples_DR110_clap__pcm_len},
    SampleData{samples_DR55HAT__pcm, samples_DR55HAT__pcm_len},
    SampleData{samples_DR55RIM__pcm, samples_DR55RIM__pcm_len},
    SampleData{samples_duo_hat_01__pcm, samples_duo_hat_01__pcm_len},
    SampleData{samples_duo_kick_01__pcm, samples_duo_kick_01__pcm_len},
    SampleData{samples_DUO_snare_01__pcm, samples_DUO_snare_01__pcm_len},
    SampleData{samples_FR_BB_Sarik_HHat_010_1__pcm, samples_FR_BB_Sarik_HHat_010_1__pcm_len},
    SampleData{samples_FR_BB_Sarik_Snare_004_1__pcm, samples_FR_BB_Sarik_Snare_004_1__pcm_len},
    SampleData{samples_Finger_Snap__pcm, samples_Finger_Snap__pcm_len},
    SampleData{samples_JR_SDD_HAT_A1_mono__pcm, samples_JR_SDD_HAT_A1_mono__pcm_len},
    SampleData{samples_JR_SDD_KICK_1_1__pcm, samples_JR_SDD_KICK_1_1__pcm_len},
    SampleData{samples_JR_SDD_SNARE_10__pcm, samples_JR_SDD_SNARE_10__pcm_len},
    SampleData{samples_KEMP8_SET1_54_004__pcm, samples_KEMP8_SET1_54_004__pcm_len},
    SampleData{samples_Kick_C78__pcm, samples_Kick_C78__pcm_len},
    SampleData{samples_Kick_909_23__pcm, samples_Kick_909_23__pcm_len},
    SampleData{samples_skclhat__pcm, samples_skclhat__pcm_len},
    SampleData{samples_Snare_909_3__pcm, samples_Snare_909_3__pcm_len},
    SampleData{samples_Snare_C78_with_silence__pcm, samples_Snare_C78_with_silence__pcm_len},
    SampleData{samples_vocal_3__pcm, samples_vocal_3__pcm_len},
    SampleData{samples_Zap_2__pcm, samples_Zap_2__pcm_len}};

struct MemorySoundSource : BufferSource {
  constexpr MemorySoundSource(const size_t sample_index)
      : sample_index(sample_index),
        reader(sample_bank[sample_index].data, sample_bank[sample_index].length),
        sound(Sound(reader)) {
  }

  void next_sample() {
    sample_index = (sample_index + 4) % 32;
    reader.set_source(sample_bank[sample_index].data, sample_bank[sample_index].length);
  }

  void fill_buffer(AudioBlock &out_samples) {
    sound.fill_buffer(out_samples);
  }

  size_t sample_index;
  PcmReader reader;
  Sound sound;
};

etl::array<MemorySoundSource, 4> sounds = {MemorySoundSource(0), MemorySoundSource(1),
                                           MemorySoundSource(2), MemorySoundSource(3)};

const etl::array<BufferSource *, 4> sources = {
    &sounds[0],
    &sounds[1],
    &sounds[2],
    &sounds[3],
};

AudioMixer mixer(sources);

Crusher crusher(mixer);

// Lowpass lowpass(mixer);
Lowpass lowpass(crusher);

// static BufferSource &master_source = mixer;
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

  const std::array pitches{0.6f, 0.3f, 1.0f, 1.9f, 1.4f};
  const std::array freqs{200.0f, 500.0f, 700.0f, 1200.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
  const std::array crush_rates{2489.0f, 44100.0f}; // Also make 44100 a float

  lowpass.filter.resonance(3.0f);

  printf("Entering main loop\n");
  while (true) {
    const auto now = to_ms_since_boot(get_absolute_time());
    const auto diff_ms = now - last_ms;
    last_ms = now;
    accum_ms += diff_ms;

    if (!AudioOutput::update(master_source)) {
      if (accum_ms > 300) {
        accum_ms = 0;
        printf("Playing sound\n");

        mixer.gain(0, 0.9);
        mixer.gain(1, 0.8);
        mixer.gain(2, 0.3);
        mixer.gain(3, 0.7);

        // Get the BufferSource pointer
        MemorySoundSource source = sounds[sound_index];
        pitch_index = (pitch_index + 1) % pitches.size();

        const auto pitch = 1;
        // const auto pitch = pitches[pitch_index];
        // Cast to Sound* before calling play()
        if (!source.reader.has_data()) {
          source.next_sample();
          source.sound.play(pitch);
        }

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

/*
consteval int test_memory_sound() {
  // samples_005__pcm, samples_005__pcm_len
  MemorySound sound1(0);
  MemorySound sound2(0);
  MemorySound sound3(1);
  MemorySound sound4(1);

  sound1.sound.play(1.0);
  AudioBlock block;
  sound1.sound.fill_buffer(block);

  return 0;
}

static_assert(test_memory_sound() == 0);
*/
