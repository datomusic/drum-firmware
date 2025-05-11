#include <pico/stdio_usb.h>
#include <pico/time.h>

#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"

#include <stdio.h>

#include "musin/audio/audio_output.h"
#include "rompler.h"

// File receiving:
// - React to some file-transfer start event. SysEx message or something else.
// - Open a source stream from the relevant transport (sysex, serial, whatever).
// - Notify sink (saving to filesystem) about start of a new transfer. Essentially opens a file.
// - Decode incoming data into bytes
// - Keep two buffers
// - Read into one buffer until full, or stream ends
// - Switch buffers, and read following bytes into second one
// - Pass the filled buffer to sink (which will write data to file)
// - If 



static void handle_sysex(byte *data, unsigned length) {
}

int main() {
  stdio_usb_init();
  musin::usb::init();

  for (int i = 0; i < 80; ++i) {
    sleep_ms(100);
    printf(".\n");
  }

  SampleBank bank;
  Rompler rompler(bank);

  MIDI::init(MIDI::Callbacks{.sysex = handle_sysex});

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
