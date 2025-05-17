#include <pico/stdio_usb.h>
#include <pico/time.h>
#include <stdio.h>

#include "musin/audio/audio_output.h"
#include "musin/audio/file_reader.h"
#include "musin/audio/mixer.h"
#include "musin/audio/sound.h"
#include "musin/filesystem/filesystem.h"
#include "musin/usb/usb.h"

#include "../../sysex/protocol.h"
#include "printing_file_ops.h"
#include "rompler.h"

#define REFORMAT_FS_ON_BOOT false

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

PrintingFileOps file_ops;
static sysex::Protocol syx_protocol(file_ops);
typedef sysex::Protocol<PrintingFileOps>::Result SyxProcotolResult;

bool received_new_file = false;

static void handle_sysex(byte *data, unsigned length) {
  const auto chunk = sysex::Chunk(data, length);
  printf("Handling sysex\n");
  const auto result = syx_protocol.handle_chunk(chunk);
  if (result == SyxProcotolResult::FileWritten) {
    received_new_file = true;
  }
  // printf("result: %i\n", result);
  // printf("State: %i\n", syx_protocol.__get_state());
  /*
  switch(result){

  }
  */
}

struct FileSound {
  FileSound() : sound(Sound(reader)) {
  }

  musin::Audio::FileReader reader;
  Sound sound;
};

FileSound tmp_sample;
FileSound tmp_sample2;

static void handle_note_on(const uint8_t, const uint8_t, const uint8_t) {
  printf("Playing sample\n");
  tmp_sample.sound.play(1);
}

const etl::array<BufferSource *, 2> sources = {&tmp_sample.sound, &tmp_sample2.sound};

AudioMixer mixer(sources);

int main() {
  stdio_usb_init();
  musin::usb::init();

  for (int i = 0; i < 80; ++i) {
    sleep_ms(100);
    printf(".\n");
  }

  printf("Initializing filesystem\n");
  const auto fs_init_result = musin::filesystem::init(REFORMAT_FS_ON_BOOT);

  if (!fs_init_result) {
    printf("Filesystem initialization failed: %i\n", fs_init_result);
  }

  SampleBank bank;
  Rompler rompler(bank);

  MIDI::init(MIDI::Callbacks{.note_on = handle_note_on, .sysex = handle_sysex});

  printf("Initializing audio output\n");
  if (!AudioOutput::init()) {
    printf("Audio initialization failed\n");
    return 1;
  }

  AudioOutput::volume(0.5);

  printf("[Rompler] Starting main loop\n");
  tmp_sample.reader.load("/tmp_sample");
  while (true) {
    musin::usb::background_update();
    MIDI::read();

    if (!syx_protocol.busy()) {
      if (received_new_file) {
        printf("Loading new sample!\n");
        tmp_sample.reader.load("/tmp_sample");
        received_new_file = false;
      }

      tmp_sample.reader.update();
      AudioOutput::update(mixer);
    }
  }
  return 0;
}
