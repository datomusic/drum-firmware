#include <pico/stdio_usb.h>
#include <pico/time.h>

#include "musin/usb/usb.h"

#include <stdio.h>

#include "../../sysex/protocol.h"
#include "musin/audio/audio_output.h"
#include "rompler.h"

// File receiving:
// - React to some file-transfer start event. SysEx message or something else.
// - Probably enter some file-transfer state, audio, sequencer etc. are disabled.
// - Open a source stream from the relevant transport (sysex, serial, whatever).
// - Notify sink (saving to filesystem) about start of a new transfer. Essentially opens a file.
// - Decode incoming data into bytes
// - Keep two buffers
// - Read into one buffer until full, or stream ends
// - Switch buffers, and read following bytes into second one
// - Pass the filled buffer to sink (which will write data to file)
// - If other buffer is filled

struct PrintingFileOps {
  static const unsigned BlockSize = 256;

  struct Handle {

    constexpr Handle() {
    }

    void close() {
      printf("Closing file!\n");
      return;
    }

    // TODO: Use Chunk instead
    constexpr size_t write(const etl::array<uint8_t, BlockSize> & /* bytes */, const size_t count) {
      printf("Writing %i bytes\n", count);
      return count;
    }
  };

  // Handle should close upon destruction
  constexpr Handle open(const char *path) {
    printf("Opening new file: %s\n", path);
    return Handle();
  }
};

PrintingFileOps file_ops;
static sysex::Protocol syx_protocol(file_ops);

static void handle_sysex(byte *data, unsigned length) {
  const auto chunk = sysex::Chunk(data, length);
  syx_protocol.handle_chunk(chunk);
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
