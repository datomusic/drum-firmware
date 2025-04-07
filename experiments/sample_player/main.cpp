#include "musin/audio/audio_output.h"
#include "musin/audio/block.h"
#include "musin/audio/buffer_source.h"
#include "musin/audio/crusher.h"
#include "musin/audio/filter.h"
#include "musin/audio/mixer.h"
#include "musin/midi/midi_wrapper.h" // Added
#include "musin/usb/usb.h"           // Added
#include "pico/time.h"
#include "samples/AudioSampleCashregister.h"
#include "samples/AudioSampleHihat.h"
#include "samples/AudioSampleKick.h"
#include "samples/AudioSampleSnare.h"
#include "musin/audio/sound.h"
#include <array>
#include <cmath> // For powf
#include <cstdint>
#include <cstdio>
#include <etl/array.h>

using namespace Musin::Audio;
using MIDI::byte;

Sound kick(AudioSampleKick, AudioSampleKickSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound cashreg(AudioSampleCashregister, AudioSampleCashregisterSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);

const std::array<Sound *, 4> sound_ptrs = {&kick, &snare, &hihat, &cashreg};

AudioMixer<4> mixer(&kick, &snare, &hihat, &cashreg);

Crusher crusher(mixer);
Lowpass lowpass(crusher);

// Output from Filter goes to AudioOutput
BufferSource &master_source = lowpass; // Send filter output

// Default MIDI channels for sounds (0-indexed)
const int KICK_CHANNEL = 9;  // Channel 10
const int SNARE_CHANNEL = 10; // Channel 11
const int HIHAT_CHANNEL = 11; // Channel 12
const int CASHREG_CHANNEL = 12;// Channel 13

// Base note for pitch calculation (C4 = 1.0 speed)
const int BASE_NOTE = 60;


void handle_note_on(byte channel, byte note, byte velocity) {
  printf("NoteOn Received: Ch%d Note%d Vel%d\n", channel + 1, note, velocity);

  // Determine which sound corresponds to the channel
  int sound_index = -1;
  if (channel == KICK_CHANNEL) sound_index = 0;
  else if (channel == SNARE_CHANNEL) sound_index = 1;
  else if (channel == HIHAT_CHANNEL) sound_index = 2;
  else if (channel == CASHREG_CHANNEL) sound_index = 3;

  if (sound_index == -1) {
      // printf("NoteOn: Ignored (Wrong Channel)\n");
      return; // Ignore notes on other channels
  }

  // Calculate pitch speed based on note number relative to BASE_NOTE
  int semitones_diff = note - BASE_NOTE;
  float pitch_speed = powf(2.0f, static_cast<float>(semitones_diff) / 12.0f);

  // Trigger the sound with the calculated pitch speed
  sound_ptrs[sound_index]->play(pitch_speed);

  printf("NoteOn: Ch%d Note%d Vel%d -> Sound%d @ Speed %.2f\n", channel + 1, note, velocity, sound_index, pitch_speed);
}

void handle_note_off(byte channel, byte note, byte velocity) {
  // No channel check needed if we don't act on note off per channel
  printf("NoteOff: Ch%d Note%d Vel%d\n", channel + 1, note, velocity);
}

void handle_cc(byte channel, byte controller, byte value) {
  // CC messages might still be global or intended for a specific channel.
  // For now, assume global control for Volume, Filter, Crusher.
  // Remove channel check if CCs are global. Add channel check if CCs are per-instrument.
  // if (channel != SOME_GLOBAL_CC_CHANNEL) return; // Example if CCs were on a specific channel

  float normalized_value = static_cast<float>(value) / 127.0f;

  printf("CC: Ch%d CC%d Val%d (Norm: %.2f)\n", channel, controller, value, normalized_value);

  switch (controller) {
  case 7: // Master Volume
    AudioOutput::volume(normalized_value);
    break;

  // Removed CC 16-19 handling (Pitch is now controlled by Note Number)

  case 75: // Filter Frequency (Global)
    lowpass.frequency(normalized_value); // Use normalized wrapper
    break;
  case 76: // Filter Resonance (Global)
    lowpass.resonance(normalized_value); // Use normalized wrapper
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

void handle_sysex(byte *const data, const unsigned length) {
    printf("SysEx received: %u bytes\n", length);
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
      .sysex = handle_sysex,
  });

  printf("Sample Player Starting with MIDI Control...\n");
  sleep_ms(1000); // Allow USB/MIDI enumeration

  if (!AudioOutput::init()) {
    printf("Audio output initialization failed!\n");
    while (true); // Halt
  }

  // Set initial parameters (can be overridden by MIDI CC)
  lowpass.frequency(1.0f); // Filter fully open
  lowpass.resonance(0.0f); // Minimum resonance
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
