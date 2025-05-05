#include "etl/array.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "musin/audio/audio_output.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"

#include "pico/time.h"

#include "musin/audio/sound.h"

void handle_cc([[maybe_unused]] const byte channel, const byte controller, const byte value) {
  // Assume global control for Volume, Filter, Crusher.

  float normalized_value = static_cast<float>(value) / 127.0f;

  // printf("CC: Ch %d CC %d Val %d (Norm: %.2f)\n", channel, controller, value, normalized_value);

  switch (controller) {
  case 7: // Master Volume
    AudioOutput::volume(normalized_value);
    break;
  default:
    break;
  }
}

void handle_sysex([[maybe_unused]] byte *data, [[maybe_unused]] const unsigned length) {
  // printf("SysEx received: %u bytes\n", length);
}

int main() {
  stdio_init_all();
  musin::usb::init();
  MIDI::init(MIDI::Callbacks{
      .note_on = nullptr,
      .note_off = nullptr,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = handle_cc,
      .pitch_bend = nullptr,
      .sysex = handle_sysex,
  });

  printf("Sample Player Starting with MIDI Control (Pitch Bend Enabled)...\n");
  sleep_ms(1000); // Allow USB/MIDI enumeration

  if (!AudioOutput::init()) {
    printf("Audio output initialization failed!\n");
    while (true) {
    } // Halt
  }
  AudioOutput::route_line_in_to_headphone(true);
  // Set initial volume (can be overridden by MIDI CC 7)
  AudioOutput::volume(1.0f);

  printf("Entering main loop\n");

  while (true) {
    musin::usb::background_update();
    MIDI::read();
  }

  // Should not be reached
  AudioOutput::deinit();
  return 0;
}
