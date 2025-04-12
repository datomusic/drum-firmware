#include "musin/audio/audio_output.h"
#include "musin/filesystem/filesystem.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"
#include <pico/stdlib.h>
#include <stdio.h>

#define SHOULD_REFORMAT false

static bool init() {
  Musin::Usb::init();
  stdio_init_all();
  sleep_ms(1000);

  // stdio over USB doesn't always work directly after statup.
  // Adding sleeps doesn't seem to help, but printing some dots does.
  for (int i = 0; i < 10; ++i) {
    printf(".\n");
  }

  printf("Startup\n");
  printf("Initializing filesystem\n");

  if (!Musin::Filesystem::init(SHOULD_REFORMAT)) {
    printf("Filesystem initialization failed\n");
    return false;
  }
  printf("Filesystem initialized\n");

  MIDI::init(MIDI::Callbacks{
      .note_on = nullptr,
      .note_off = nullptr,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = nullptr,
      .pitch_bend = nullptr,
      .sysex = nullptr,
  });

  return true;
}

int main(void) {
  if (!init()) {
    printf("Init failed!\n");
    return 1;
  }

  printf("Initializing audio output\n");
  AudioOutput::init();

  printf("Entering main loop!\n");

  while (true) {
    // AudioOutput::update(fill_audio_buffer);
    Musin::Usb::background_update();
    MIDI::read(1);
  }
}
