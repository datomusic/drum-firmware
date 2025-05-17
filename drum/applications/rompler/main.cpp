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

    constexpr Handle(const char *path) {
      if (file_pointer) {
        fclose(file_pointer);
        // TODO: Report some error. This should not happen;
      }

      file_pointer = fopen(path, "wb");
      if (!file_pointer) {
        printf("ERROR: Failed opening file\n");
      }
    }

    void close() {
      printf("Closing file!\n");
      if (file_pointer) {
        fclose(file_pointer);
        file_pointer = nullptr;
      } else {
        // TODO: Error closing a non-existent handle
      }
      return;
    }

    // TODO: Use Chunk instead
    constexpr size_t write(const etl::array<uint8_t, BlockSize> &bytes, const size_t count) {
      printf("Writing %i bytes\n", count);
      if (file_pointer) {
        const auto written = fwrite(bytes.cbegin(), sizeof(uint8_t), count, file_pointer);
        printf("written: %i\n", written);
        return written;
      } else {
        // TODO: Error writing to a handle that should be exist.
        return 0;
      }
    }

  private:
    FILE *file_pointer = nullptr;
  };

  // Handle should close upon destruction
  // TODO: Return optional instead, if handle could not be opened.
  constexpr Handle open(const char *path) {
    printf("Opening new file: %s\n", path);
    return Handle(path);
  }
};

PrintingFileOps file_ops;
static sysex::Protocol syx_protocol(file_ops);

static void handle_sysex(byte *data, unsigned length) {
  const auto chunk = sysex::Chunk(data, length);
  printf("Handling sysex\n");
  const auto result = syx_protocol.handle_chunk(chunk);
  printf("result: %i\n", result);
  printf("State: %i\n", syx_protocol.__get_state());
  /*
  switch(result){

  }
  */
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

  printf("[Rompler] Starting main loop\n");
  while (true) {
    musin::usb::background_update();
    MIDI::read();
    sleep_ms(1);
  }
  return 0;
}
