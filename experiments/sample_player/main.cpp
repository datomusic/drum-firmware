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
#include <cstdint>
#include <cstdio>
#include <etl/array.h>

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

// MIDI channel (1-indexed for internal use, corresponds to channel 1)
#define MIDI_CHANNEL 1

// --- Pitch storage ---
// Store the last received pitch speed for each sound (initialized to 1.0)
std::array<float, 4> current_pitches = {1.0f, 1.0f, 1.0f, 1.0f};


void handle_note_on(byte channel, byte note, byte velocity) {
  printf("NoteOn Received: Ch%d Note%d Vel%d\n", channel, note, velocity);
  if (channel != MIDI_CHANNEL) return;

  // Map MIDI note 60+ to sound index 0+
  int sound_index = (note - 60);
  if (sound_index < 0 || sound_index >= sound_ptrs.size()) {
      printf("NoteOn: Ch%d Note%d Vel%d -> Ignored (Out of range)\n", channel, note, velocity);
      return;
  }

  // Retrieve the stored pitch speed for this sound
  float pitch_speed_to_play = current_pitches[sound_index];
  sound_ptrs[sound_index]->play(pitch_speed_to_play);

  printf("NoteOn: Ch%d Note%d Vel%d -> Sound%d @ Speed %.2f\n", channel, note, velocity, sound_index, pitch_speed_to_play);
}

void handle_note_off(byte channel, byte note, byte velocity) {
  if (channel != MIDI_CHANNEL) return;
  printf("NoteOff: Ch%d Note%d Vel%d\n", channel, note, velocity);
}

void handle_cc(byte channel, byte controller, byte value) {
  if (channel != MIDI_CHANNEL) return;

  float normalized_value = static_cast<float>(value) / 127.0f;

  printf("CC: Ch%d CC%d Val%d (Norm: %.2f)\n", channel, controller, value, normalized_value);

  switch (controller) {
  case 7: // Master Volume
    AudioOutput::volume(normalized_value);
    break;

  case 16: // Pitch Sound 1 (Kick)
  case 17: // Pitch Sound 2 (Snare)
  case 18: // Pitch Sound 3 (Hihat)
  case 19: // Pitch Sound 4 (Cashreg)
  {
      int sound_idx = controller - 16;
      if (sound_idx >= 0 && sound_idx < sound_ptrs.size()) {
          // Map normalized_value [0.0, 1.0] to desired pitch speed range, e.g., [0.1, 4.0]
          const float min_speed = 0.1f;
          const float max_speed = 4.0f;
          float target_speed = min_speed + normalized_value * (max_speed - min_speed);
          // Store the calculated pitch speed
          current_pitches[sound_idx] = target_speed;
          printf("  -> Stored Pitch Speed %.2f for Sound %d\n", target_speed, sound_idx);
      }
  }
  break;

  case 75: // Filter Frequency
    lowpass.frequency(normalized_value); // Use the Lowpass wrapper method
    break;
  case 76: // Filter Resonance
    lowpass.resonance(normalized_value); // Use the Lowpass wrapper method
    break;

  case 77: // Crusher Bits (Squish)
    crusher.squish(normalized_value);
    break;
  case 78: // Crusher Rate (Squeeze)
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
  lowpass.filter.frequency(10000.0f);
  lowpass.filter.resonance(0.0f);
  crusher.squish(0.0f);   // No bit crush
  crusher.squeeze(0.0f);  // No rate crush

  // Set initial volume (can be overridden by MIDI CC 7)
  AudioOutput::volume(1.0f); // Default volume

  printf("Entering main loop\n");

  while (true) {
    Musin::Usb::background_update();
    MIDI::read(MIDI_CHANNEL);
    AudioOutput::update(master_source);
  }

  // Should not be reached
  AudioOutput::deinit();
  return 0;
}
