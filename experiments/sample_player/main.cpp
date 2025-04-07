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
#include <array> // Use std::array for consistency
#include <cstdint>
#include <cstdio>
#include <etl/array.h>

// --- Audio Setup ---
Sound kick(AudioSampleKick, AudioSampleKickSize);
Sound snare(AudioSampleSnare, AudioSampleSnareSize);
Sound cashreg(AudioSampleCashregister, AudioSampleCashregisterSize);
Sound hihat(AudioSampleHihat, AudioSampleHihatSize);

// Store pointers for MIDI access
const std::array<Sound *, 4> sound_ptrs = {&kick, &snare, &hihat, &cashreg};

// Construct mixer directly with the source pointers
AudioMixer<4> mixer(&kick, &snare, &hihat, &cashreg);

// Apply effects in the desired order
Crusher crusher(mixer); // Mixer -> Crusher
Lowpass filter(crusher); // Crusher -> Filter

// Use the final effect in the chain as the master source
BufferSource &master_source = filter; // Output from Filter goes to AudioOutput

// --- MIDI Configuration ---
#define MIDI_CHANNEL 1

// --- MIDI Callback Functions ---

void handle_note_on(byte channel, byte note, byte velocity) {
  printf("NoteOn Received: Ch%d Note%d Vel%d\n", channel, note, velocity);
  if (channel != MIDI_CHANNEL) return;

  // Map MIDI note number to sound index (e.g., notes 36, 37, 38, 39)
  // Adjust the base note (36) as needed.
  int sound_index = (note - 36);
  if (sound_index < 0 || sound_index >= sound_ptrs.size()) {
      printf("NoteOn: Ch%d Note%d Vel%d -> Ignored (Out of range)\n", channel, note, velocity);
      return; // Ignore notes outside the mapped range
  }


  // Trigger the sound using the simple play() method. Pitch is set via CC.
  sound_ptrs[sound_index]->play();
}

void handle_note_off(byte channel, byte note, byte velocity) {
  if (channel != MIDI_CHANNEL) return;
  // Currently no action on Note Off, samples play to completion.
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

  // --- Pitch Control ---
  case 16: // Pitch Sound 1 (Kick)
  case 17: // Pitch Sound 2 (Snare)
  case 18: // Pitch Sound 3 (Hihat)
  case 19: // Pitch Sound 4 (Cashreg)
  {
      int sound_idx = controller - 16;
      if (sound_idx >= 0 && sound_idx < sound_ptrs.size()) {
          sound_ptrs[sound_idx]->pitch(normalized_value);
          printf("  -> Pitch Sound %d\n", sound_idx);
      }
  }
  break;

  // --- Filter Control ---
  case 75: // Filter Frequency
    filter.frequency(normalized_value);
    break;
  case 76: // Filter Resonance
    filter.resonance(normalized_value);
    break;

  // --- Crusher Control ---
  case 77: // Crusher Bits (Squish)
    crusher.squish(normalized_value);
    break;
  case 78: // Crusher Rate (Squeeze)
    crusher.squeeze(normalized_value);
    break;

  default:
    // Unassigned CC
    break;
  }
}

void handle_sysex(byte *const data, const unsigned length) {
    // Handle SysEx if needed
    printf("SysEx received: %u bytes\n", length);
}


int main() {
  stdio_init_all();
  Musin::Usb::init(); // Initialize USB first
  MIDI::init(MIDI::Callbacks{ // Initialize MIDI with callbacks
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

  // Initialize audio output
  if (!AudioOutput::init()) {
    printf("Audio output initialization failed!\n");
    while (true)
      ; // Halt
  }

  // Set initial parameters for effects (can be overridden by MIDI CC)
  filter.frequency(1.0f); // Default 50% frequency
  filter.resonance(0.0f); // Default minimum resonance (0.707)
  crusher.squish(0.0f);   // No bit crush initially (16 bits)
  crusher.squeeze(0.0f);  // No rate crush initially (SAMPLE_FREQ)

  // Set initial master volume (can be overridden by MIDI CC 7)
  AudioOutput::volume(0.3f);

  printf("Entering main loop\n");

  while (true) {
    // Handle background tasks for USB
    Musin::Usb::background_update();

    // Read and process incoming MIDI messages
    MIDI::read(MIDI_CHANNEL);

    // Update audio output (fetches data through the chain)
    AudioOutput::update(master_source);

    // No automatic triggering needed anymore
    // The old timer-based logic is removed.
  }

  AudioOutput::deinit(); // Should not be reached
  return 0;
}
