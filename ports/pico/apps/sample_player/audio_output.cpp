#include "audio_output.h"
#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"

static audio_buffer_pool_t *producer_pool;
static bool running = false;
static AudioOutput::BufferCallback buffer_callback = nullptr;

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define SAMPLE_FREQUENCY 44100
#define BUFFER_COUNT 3

static audio_format_t audio_format = {.sample_freq = SAMPLE_FREQUENCY,
                                      .pcm_format = AUDIO_PCM_FORMAT_S32,
                                      .channel_count = AUDIO_CHANNEL_STEREO};

static audio_buffer_format_t producer_format = {
    .format = &audio_format,
    .sample_stride = 8}; // 4 bytes per sample, stereo, which means 8 bytes per
                         // stereo sampling point

static audio_i2s_config_t i2s_config = {.data_pin = PICO_AUDIO_I2S_DATA_PIN,
                                        .clock_pin_base =
                                            PICO_AUDIO_I2S_CLOCK_PIN_BASE,
                                        .dma_channel0 = 0,
                                        .dma_channel1 = 1,
                                        .pio_sm = 0};

void AudioOutput::init(BufferCallback callback) {
  buffer_callback = callback;
  audio_format.sample_freq = SAMPLE_FREQUENCY;

  producer_pool = audio_new_producer_pool(&producer_format, BUFFER_COUNT,
                                          AUDIO_BLOCK_SAMPLES);

  bool __unused ok;
  const audio_format_t *output_format;

  output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);

  for (int i = 0; i < BUFFER_COUNT; ++i) {
    callback(producer_pool);
  }

  audio_i2s_set_enabled(true);

  running = true;
}

void AudioOutput::deinit() {
  running = false;

  audio_i2s_set_enabled(false);
  audio_i2s_end();

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

extern "C" {
// callback from:
//   void __isr __time_critical_func(audio_i2s_dma_irq_handler)()
//   defined at my_pico_audio_i2s/audio_i2s.c
//   where i2s_callback_func() is declared with __attribute__((weak))
void i2s_callback_func() {
  if (running) {
    if (buffer_callback) {
      buffer_callback(producer_pool);
    }
  }
}
}
