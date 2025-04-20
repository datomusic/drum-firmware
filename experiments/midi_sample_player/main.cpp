#include "etl/array.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "musin/audio/audio_output.h"
#include "musin/audio/buffer_source.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"
#include "samples.h"

#include "pico/time.h"

#include "samples/AudioSampleClapdr110_16bit_44kw.h"
#include "samples/AudioSampleHatdr55_16bit_44kw.h"
#include "samples/AudioSampleKickc78_16bit_44kw.h"
#include "samples/AudioSampleSnare100_16bit_44kw.h"

#include "musin/audio/audio_memory_reader.h"
#include "musin/audio/file_reader.h"
#include "musin/audio/sound.h"

struct SampleData {
  unsigned char *data;
  size_t length;
};

constexpr const size_t SampleCount = 1;
SampleData sample_bank[SampleCount] = {
    {samples_005__pcm, samples_005__pcm_len},
    /*
    {samples_006__pcm, samples_006__pcm_len},
    {samples_015__pcm, samples_015__pcm_len},
    {samples_100_nt_snare__pcm, samples_100_nt_snare__pcm_len},
    {samples_26880__vexst__closed_hi_hat_2_1__pcm,
     samples_26880__vexst__closed_hi_hat_2_1__pcm_len},
    {samples_26887__vexst__kick_3_1__pcm, samples_26887__vexst__kick_3_1__pcm_len},
    {samples_26901__vexst__snare_2_1__pcm, samples_26901__vexst__snare_2_1__pcm_len},
    {samples_44_Analog_Cowbell__pcm, samples_44_Analog_Cowbell__pcm_len},
    {samples_cabasa__pcm, samples_cabasa__pcm_len},
    {samples_chihiro_snare__pcm, samples_chihiro_snare__pcm_len},
    {samples_cowbell_hi__pcm, samples_cowbell_hi__pcm_len},
    {samples_ClosedHH_909X_2__pcm, samples_ClosedHH_909X_2__pcm_len},
    {samples_DR110_clap__pcm, samples_DR110_clap__pcm_len},
    {samples_DR55HAT__pcm, samples_DR55HAT__pcm_len},
    {samples_DR55RIM__pcm, samples_DR55RIM__pcm_len},
    {samples_duo_hat_01__pcm, samples_duo_hat_01__pcm_len},
    {samples_duo_kick_01__pcm, samples_duo_kick_01__pcm_len},
    {samples_DUO_snare_01__pcm, samples_DUO_snare_01__pcm_len},
    {samples_FR_BB_Sarik_HHat_010_1__pcm, samples_FR_BB_Sarik_HHat_010_1__pcm_len},
    {samples_FR_BB_Sarik_Snare_004_1__pcm, samples_FR_BB_Sarik_Snare_004_1__pcm_len},
    {samples_Finger_Snap__pcm, samples_Finger_Snap__pcm_len},
    {samples_JR_SDD_HAT_A1_mono__pcm, samples_JR_SDD_HAT_A1_mono__pcm_len},
    {samples_JR_SDD_KICK_1_1__pcm, samples_JR_SDD_KICK_1_1__pcm_len},
    {samples_JR_SDD_SNARE_10__pcm, samples_JR_SDD_SNARE_10__pcm_len},
    {samples_KEMP8_SET1_54_004__pcm, samples_KEMP8_SET1_54_004__pcm_len},
    {samples_Kick_C78__pcm, samples_Kick_C78__pcm_len},
    {samples_Kick_909_23__pcm, samples_Kick_909_23__pcm_len},
    {samples_skclhat__pcm, samples_skclhat__pcm_len},
    {samples_Snare_909_3__pcm, samples_Snare_909_3__pcm_len},
    {samples_Snare_C78_with_silence__pcm, samples_Snare_C78_with_silence__pcm_len},
    {samples_vocal_3__pcm, samples_vocal_3__pcm_len},
    {samples_Zap_2__pcm, samples_Zap_2__pcm_len}};
    */
    };

using Musin::Audio::FileReader;

struct MemorySound {
  MemorySound(const unsigned int *sample_data, const uint32_t data_length)
      : reader(sample_data, data_length), sound(Sound(reader)) {
  }

  AudioMemoryReader reader;
  Sound sound;
};

FileReader reader;

MemorySound kick(AudioSampleKickc78_16bit_44kw, AudioSampleKickc78_16bit_44kwSize);
MemorySound snare(AudioSampleSnare100_16bit_44kw, AudioSampleSnare100_16bit_44kwSize);
MemorySound clap(AudioSampleClapdr110_16bit_44kw, AudioSampleClapdr110_16bit_44kwSize);
MemorySound hihat(AudioSampleHatdr55_16bit_44kw, AudioSampleHatdr55_16bit_44kwSize);

const etl::array<BufferSource *, 4> sound_ptrs = {&kick.sound, &snare.sound, &hihat.sound,
                                                  &clap.sound};

AudioMixer<4> mixer(sound_ptrs);

Crusher crusher(mixer);
Lowpass lowpass(crusher);

BufferSource &master_source = lowpass;

// Default MIDI channels for sounds (1-indexed)
constexpr std::uint8_t KICK_CHANNEL = 10;
constexpr std::uint8_t SNARE_CHANNEL = 11;
constexpr std::uint8_t HIHAT_CHANNEL = 12;
constexpr std::uint8_t CLAP_CHANNEL = 13;

// Array to store the current pitch speed for each sound channel, controlled by Pitch Bend
// Index mapping: 0=Kick, 1=Snare, 2=Hihat, 3=Clap
etl::array<float, 4> channel_pitch_speed = {1.0f, 1.0f, 1.0f, 1.0f};

void handle_note_on(const byte channel, [[maybe_unused]] const byte note,
                    [[maybe_unused]] const byte velocity) {
  // printf("NoteOn Received: Ch %d Note %d Vel %d\n", channel, note, velocity);

  int sound_index = -1;
  if (channel == KICK_CHANNEL)
    sound_index = 0;
  else if (channel == SNARE_CHANNEL)
    sound_index = 1;
  else if (channel == HIHAT_CHANNEL)
    sound_index = 2;
  else if (channel == CLAP_CHANNEL)
    sound_index = 3;

  if (sound_index == -1) {
    return; // Ignore notes on other channels
  }

  // Retrieve the current pitch speed for this channel (set by pitch bend)
  float pitch_speed = channel_pitch_speed[sound_index];

  // Trigger the sound with the current pitch speed
  static_cast<Sound *>(sound_ptrs[sound_index])->play(pitch_speed);

  // printf("NoteOn: Ch %d Note %d Vel %d -> Sound %d @ Speed %.2f\n", channel, note, velocity,
  // sound_index, pitch_speed);
}

void handle_note_off([[maybe_unused]] const byte channel, [[maybe_unused]] const byte note,
                     [[maybe_unused]] const byte velocity) {
  // printf("NoteOff: Ch %d Note %d Vel %d\n", channel, note, velocity);
}

void handle_cc([[maybe_unused]] const byte channel, const byte controller, const byte value) {
  // Assume global control for Volume, Filter, Crusher.

  float normalized_value = static_cast<float>(value) / 127.0f;

  // printf("CC: Ch %d CC %d Val %d (Norm: %.2f)\n", channel, controller, value, normalized_value);

  switch (controller) {
  case 7: // Master Volume
    AudioOutput::volume(normalized_value);
    break;

  case 75: // Filter Frequency (Global) - Exponential Scaling
  {
    const float min_freq = 20.0f;
    const float max_freq = 10000.0f;
    float freq = min_freq * powf(max_freq / min_freq, normalized_value);
    lowpass.filter.frequency(freq);
  } break;
  case 76: // Filter Resonance (Global)
    lowpass.filter.resonance(normalized_value * 5.0f);
    break;

  case 77: // Crusher Bits (Squish) (Global)
    crusher.squish(normalized_value);
    break;
  case 78: // Crusher Rate (Squeeze) (Global)
    crusher.squeeze(normalized_value);
    break;

  default:
    break;
  }
}

void handle_pitch_bend(const byte channel, const int bend) {
  // MIDI pitch bend value is 14-bit (0-16383), center is 8192.

  // Determine which sound corresponds to the channel
  int sound_index = -1;
  if (channel == KICK_CHANNEL)
    sound_index = 0;
  else if (channel == SNARE_CHANNEL)
    sound_index = 1;
  else if (channel == HIHAT_CHANNEL)
    sound_index = 2;
  else if (channel == CLAP_CHANNEL)
    sound_index = 3;

  if (sound_index == -1) {
    return; // Ignore pitch bend on other channels
  }

  // Normalize bend value from 0-16383 to -1.0 to +1.0
  // 8192 is center (0.0), 0 is min (-1.0), 16383 is max (+1.0)
  float normalized_bend = (static_cast<float>(bend) - 8192.0f) / 8191.0f;

  // Map normalized bend (-1 to +1) to speed (0.5 to 2.0) exponentially
  // This corresponds to a pitch range of +/- 1 octave.
  // speed = 2 ^ normalized_bend
  float speed = powf(2.0f, normalized_bend);

  // Store the calculated speed for the channel
  channel_pitch_speed[sound_index] = speed;

  // printf("PitchBend: Ch %d Bend %d -> Sound %d Speed %.3f\n", channel, bend, sound_index, speed);
}

void handle_sysex([[maybe_unused]] byte *data, [[maybe_unused]] const unsigned length) {
  // printf("SysEx received: %u bytes\n", length);
}

int main() {
  stdio_init_all();
  Musin::Usb::init();
  MIDI::init(MIDI::Callbacks{
      .note_on = handle_note_on,
      .note_off = handle_note_off,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = handle_cc,
      .pitch_bend = handle_pitch_bend,
      .sysex = handle_sysex,
  });

  printf("Sample Player Starting with MIDI Control (Pitch Bend Enabled)...\n");
  sleep_ms(1000); // Allow USB/MIDI enumeration

  if (!AudioOutput::init()) {
    printf("Audio output initialization failed!\n");
    while (true) {
    } // Halt
  }

  // Set initial parameters (can be overridden by MIDI CC)
  lowpass.filter.frequency(10000.0f);
  lowpass.filter.resonance(0.0f); // Minimum resonance
  crusher.squish(0.0f);           // No bit crush
  crusher.squeeze(0.0f);          // No rate crush

  // Set initial volume (can be overridden by MIDI CC 7)
  AudioOutput::volume(1.0f);

  printf("Entering main loop\n");

  while (true) {
    Musin::Usb::background_update();
    MIDI::read();
    AudioOutput::update(master_source);
  }

  // Should not be reached
  AudioOutput::deinit();
  return 0;
}
