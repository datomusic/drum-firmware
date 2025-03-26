#include "musin/audio/mixer.h"
#include "musin/filesystem/filesystem.h"
#include "musin/midi/midi_wrapper.h"
#include "musin/usb/usb.h"

// TODO: Pico stdio stuff should live in shared code
#include <pico/stdlib.h>

#include <stdio.h>

#define SHOULD_REFORMAT false

static const uint master_volume = 10;

static bool init() {
  stdio_init_all();

  Musin::Usb::init();
  MIDI::init(MIDI::Callbacks{
      .note_on = nullptr,
      .note_off = nullptr,
      .clock = nullptr,
      .start = nullptr,
      .cont = nullptr,
      .stop = nullptr,
      .cc = nullptr,
      .sysex = nullptr,
  });

  // Give host some time to catch up, otherwise messages can be lost.
  sleep_ms(2000);

  printf("Startup\n");
  printf("\n\n");
  printf("Initializing fs\n");
  const auto init_result = init_filesystem(SHOULD_REFORMAT);
  if (!init_result) {
    printf("Initialization failed: %i\n", init_result);
    return false;
  }

  printf("file system initialized\n");
  return true;
}

static void __not_in_flash_func(fill_audio_buffer)(audio_buffer_t *out_buffer) {

  static int16_t temp_samples[AUDIO_BLOCK_SAMPLES];
  // mixer.fill_buffer(temp_samples);

  int16_t *stereo_out_samples = (int16_t *)out_buffer->buffer->bytes;
  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    stereo_out_samples[i] = (master_volume * temp_samples[i]) >> 8u;
  }

  out_buffer->sample_count = AUDIO_BLOCK_SAMPLES;
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
