#include <pico/stdio_usb.h>
#include <pico/time.h>

#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"

#include <stdio.h>

#include "musin/audio/audio_output.h"
#include "rompler.h"

int main() {
  stdio_usb_init();
  musin::usb::init();

  for (int i = 0; i < 80; ++i) {
    sleep_ms(100);
    printf(".\n");
  }

  SampleBank bank;
  Rompler rompler(bank);

  MIDI::init(MIDI::Callbacks{});

  printf("Initializing audio output\n");
  if (!AudioOutput::init()) {
    printf("Audio initialization failed\n");
    return 1;
  }

  printf("Starting main loop\n");
  while (true) {
    musin::usb::background_update();
    sleep_ms(1);
  }
  return 0;
}
