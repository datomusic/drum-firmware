#include "musin/audio/audio_output.h"
#include "musin/audio/block.h"
#include "musin/audio/buffer_source.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"
#include "pico/time.h"
#include "samples/AudioSampleClapdr110_16bit_44kw.h"
#include "samples/AudioSampleSnare100_16bit_44kw.h"
#include "samples/AudioSampleKickc78_16bit_44kw.h"
#include "samples/AudioSampleHatdr55_16bit_44kw.h"
#include "musin/audio/sound.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <etl/array.h>

Sound kick(AudioSampleKickc78_16bit_44kw, AudioSampleKickc78_16bit_44kwSize);
Sound snare(AudioSampleSnare100_16bit_44kw, AudioSampleSnare100_16bit_44kwSize);
Sound clap(AudioSampleClapdr110_16bit_44kw, AudioSampleClapdr110_16bit_44kwSize);
Sound hihat(AudioSampleHatdr55_16bit_44kw, AudioSampleHatdr55_16bit_44kwSize);

const std::array<Sound *, 4> sound_ptrs = {&kick, &snare, &hihat, &clap};

AudioMixer<4> mixer(&kick, &snare, &hihat, &clap);

Crusher crusher(mixer);
Lowpass lowpass(crusher);

// Output from Filter goes to AudioOutput
BufferSource &master_source = lowpass; // Send filter output

// Default MIDI channels for sounds (1-indexed)
const int KICK_CHANNEL = 10;  // Channel 10
const int SNARE_CHANNEL = 11; // Channel 11
const int HIHAT_CHANNEL = 12; // Channel 12
const int CLAP_CHANNEL = 13;// Channel 13

// Array to store the current pitch speed for each sound channel, controlled by Pitch Bend
// Index mapping: 0=Kick, 1=Snare, 2=Hihat, 3=Clap
std::array<float, 4> channel_pitch_speed = {1.0f, 1.0f, 1.0f, 1.0f};


void handle_note_on(byte channel, byte note, byte velocity) {
  //printf("NoteOn Received: Ch %d Note %d Vel %d\n", channel, note, velocity);

  // Determine which sound corresponds to the channel
  int sound_index = -1;
  if (channel == KICK_CHANNEL) sound_index = 0;
  else if (channel == SNARE_CHANNEL) sound_index = 1;
  else if (channel == HIHAT_CHANNEL) sound_index = 2;
  else if (channel == CLAP_CHANNEL) sound_index = 3;

  if (sound_index == -1) {
      // printf("NoteOn: Ignored (Wrong Channel)\n");
      return; // Ignore notes on other channels
  }

  // Retrieve the current pitch speed for this channel (set by pitch bend)
  float pitch_speed = channel_pitch_speed[sound_index];

  // Trigger the sound with the current pitch speed
  sound_ptrs[sound_index]->play(pitch_speed);

  // printf("NoteOn: Ch %d Note %d Vel %d -> Sound %d @ Speed %.2f\n", channel, note, velocity, sound_index, pitch_speed);
}

void handle_note_off(byte channel, byte note, byte velocity) {
  //printf("NoteOff: Ch %d Note %d Vel %d\n", channel, note, velocity);
}

void handle_cc(byte channel, byte controller, byte value) {
  // Assume global control for Volume, Filter, Crusher.

  float normalized_value = static_cast<float>(value) / 127.0f;

  //printf("CC: Ch %d CC %d Val %d (Norm: %.2f)\n", channel, controller, value, normalized_value);

  switch (controller) {
  case 7: // Master Volume
    AudioOutput::volume(normalized_value);
    break;

  case 75: // Filter Frequency (Global)
    lowpass.filter.frequency(normalized_value * 10000.0f); 
    break;
  case 76: // Filter Resonance (Global)
    lowpass.filter.resonance(normalized_value);
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

void handle_pitch_bend(byte channel, int bend) {
    // MIDI pitch bend value is 14-bit (0-16383), center is 8192.

    // Determine which sound corresponds to the channel
    int sound_index = -1;
    if (channel == KICK_CHANNEL) sound_index = 0;
    else if (channel == SNARE_CHANNEL) sound_index = 1;
    else if (channel == HIHAT_CHANNEL) sound_index = 2;
    else if (channel == CLAP_CHANNEL) sound_index = 3;

    if (sound_index == -1) {
        // printf("PitchBend: Ignored (Wrong Channel %d)\n", channel);
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


void handle_sysex(byte *const data, const unsigned length) {
    //printf("SysEx received: %u bytes\n", length);
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
      .pitch_bend = handle_pitch_bend, // Add pitch bend handler
      .sysex = handle_sysex,
  });

  printf("Sample Player Starting with MIDI Control (Pitch Bend Enabled)...\n");
  sleep_ms(1000); // Allow USB/MIDI enumeration

  if (!AudioOutput::init()) {
    printf("Audio output initialization failed!\n");
    while (true); // Halt
  }

  // Set initial parameters (can be overridden by MIDI CC)
  lowpass.filter.frequency(10000.0f); 
  lowpass.filter.resonance(0.0f); // Minimum resonance
  crusher.squish(0.0f);   // No bit crush
  crusher.squeeze(0.0f);  // No rate crush

  // Set initial volume (can be overridden by MIDI CC 7)
  AudioOutput::volume(1.0f); // Default volume

  printf("Entering main loop\n");

  while (true) {
    Musin::Usb::background_update();
    MIDI::read(); // Read all channels
    AudioOutput::update(master_source);
  }

  // Should not be reached
  AudioOutput::deinit();
  return 0;
}
