#include "audio_output.h"
#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"
#include <stdio.h>

static audio_buffer_pool_t *producer_pool;
static bool running = false;

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define SAMPLE_FREQUENCY 44100
// #define SAMPLE_FREQUENCY 24000
#define BUFFER_COUNT 3

static audio_format_t audio_format = {.sample_freq = SAMPLE_FREQUENCY,
                                      .format = AUDIO_BUFFER_FORMAT_PCM_S16,
                                      .channel_count = 1};

static audio_buffer_format_t producer_format = {.format = &audio_format,
                                                .sample_stride = 2};

struct audio_i2s_config i2s_config = {
    .data_pin = PICO_AUDIO_I2S_DATA_PIN,
    .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
    .dma_channel = 0,
    .pio_sm = 0,
};

void AudioOutput::init() {
  audio_format.sample_freq = SAMPLE_FREQUENCY;

  producer_pool = audio_new_producer_pool(&producer_format, BUFFER_COUNT,
                                          AUDIO_BLOCK_SAMPLES);

  bool __unused ok;
  const audio_format_t *output_format;

  output_format = audio_i2s_setup(&audio_format, &i2s_config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);

  /*
  for (int i = 0; i < BUFFER_COUNT; ++i) {
    callback(producer_pool);
  }
  */

  audio_i2s_set_enabled(true);

  running = true;
}

void AudioOutput::deinit() {
  running = false;

  audio_i2s_set_enabled(false);

  audio_buffer_t *buffer;

  for (int i = 0; i < BUFFER_COUNT; ++i) {
    buffer = take_audio_buffer(producer_pool, false);
    while (buffer != nullptr) {
      free(buffer->buffer->bytes);
      free(buffer->buffer);
      buffer = take_audio_buffer(producer_pool, false);
    }
  }
  free(producer_pool);
  producer_pool = nullptr;
}

bool AudioOutput::update(AudioOutput::BufferCallback callback) {
  if (running) {
    // printf("RUNNING\n");
    audio_buffer_t *buffer = take_audio_buffer(producer_pool, false);
    if (buffer != nullptr) {
      // printf("GOT BUFFER\n");
      callback(buffer);
      give_audio_buffer(producer_pool, buffer);
      return false;
    }
  }

  return false;
}
