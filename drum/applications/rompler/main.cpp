#include <hardware/sync.h>
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
#include "standard_file_ops.h"

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

StandardFileOps file_ops;
static sysex::Protocol syx_protocol(file_ops);
typedef sysex::Protocol<StandardFileOps>::Result SyxProcotolResult;

bool received_new_file = false;

static void handle_sysex(byte *data, unsigned length) {
  const auto chunk = sysex::Chunk(data, length);
  const auto result = syx_protocol.handle_chunk(chunk);
  if (result == SyxProcotolResult::FileWritten) {
    received_new_file = true;
  }
}

struct FileSound {
  FileSound() : sound(Sound(reader)) {
  }

  musin::Audio::FileReader reader;
  Sound sound;
};

FileSound sample0;
FileSound sample1;
FileSound sample2;
FileSound sample3;

etl::array<FileSound *, 4> sounds = {&sample0, &sample1, &sample2, &sample3};
const etl::array<BufferSource *, 4> sources = {&sample0.sound, &sample1.sound, &sample2.sound,
                                               &sample3.sound};

static void load_samples() {
  sounds[0]->reader.load("/sample_0");
  sounds[1]->reader.load("/sample_1");
  sounds[2]->reader.load("/sample_2");
  sounds[3]->reader.load("/sample_3");
}

void handle_note_on(byte, byte note, byte) {
  printf("Received midi note %d\n", note);
  // const float pitch = (float)(velocity) / 64.0;
  const unsigned sound_index = (note % 4);
  printf("Playing sound: %i\n", sound_index);
  sounds[sound_index]->sound.play(1);
}

AudioMixer mixer(sources);

int main() {
  stdio_usb_init();
  musin::usb::init(true);

  for (int i = 0; i < 80; ++i) {
    sleep_ms(100);
    printf(".\n");
  }

  printf("Initializing filesystem\n");
  const auto fs_init_result = musin::filesystem::init(REFORMAT_FS_ON_BOOT);

  if (!fs_init_result) {
    printf("Filesystem initialization failed: %i\n", fs_init_result);
  }

  MIDI::init(MIDI::Callbacks{.note_on = handle_note_on, .sysex = handle_sysex});

  printf("Initializing audio output\n");
  if (!AudioOutput::init()) {
    printf("Audio initialization failed\n");
    return 1;
  }

  AudioOutput::volume(0.7);

  printf("[Rompler] Starting main loop\n");

  load_samples();

  while (true) {
    musin::usb::background_update();
    MIDI::read();

    if (!syx_protocol.busy()) {
      if (received_new_file) {
        printf("Loading new sample!\n");
        load_samples();
        received_new_file = false;
      }

      for (auto sound : sounds) {
        if (sound->reader.needs_update()) {
          const auto status = save_and_disable_interrupts();
          sound->reader.update();
          restore_interrupts(status);
        }
      }

      AudioOutput::update(mixer);
    }
  }
  return 0;
}
