#include "audio_output.h"
#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"

static audio_buffer_pool_t *producer_pool;
static bool running = false;

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define SAMPLE_FREQUENCY 44100

static audio_format_t audio_format = {.sample_freq = SAMPLE_FREQUENCY,
                                      .pcm_format = AUDIO_PCM_FORMAT_S32,
                                      .channel_count = AUDIO_CHANNEL_STEREO};

static audio_buffer_format_t producer_format = {.format = &audio_format,
                                                .sample_stride = 8};

static audio_i2s_config_t i2s_config = {.data_pin = PICO_AUDIO_I2S_DATA_PIN,
                                        .clock_pin_base =
                                            PICO_AUDIO_I2S_CLOCK_PIN_BASE,
                                        .dma_channel0 = 0,
                                        .dma_channel1 = 1,
                                        .pio_sm = 0};

namespace AudioOutput {
static BufferCallback buffer_callback = nullptr;

void init(BufferCallback callback, const uint32_t samples_per_buffer) {
  buffer_callback = callback;
  audio_format.sample_freq = SAMPLE_FREQUENCY;

  producer_pool = audio_new_producer_pool(&producer_format, 3, samples_per_buffer);

  bool __unused ok;
  const audio_format_t *output_format;

  output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);
  callback(producer_pool);
  callback(producer_pool);
  callback(producer_pool);
  audio_i2s_set_enabled(true);

  running = true;
}

void deinit() {
  running = false;

  audio_i2s_set_enabled(false);
  audio_i2s_end();

  audio_buffer_t *ab;
  ab = take_audio_buffer(producer_pool, false);
  while (ab != nullptr) {
    free(ab->buffer->bytes);
    free(ab->buffer);
    ab = take_audio_buffer(producer_pool, false);
  }
  ab = get_free_audio_buffer(producer_pool, false);
  while (ab != nullptr) {
    free(ab->buffer->bytes);
    free(ab->buffer);
    ab = get_free_audio_buffer(producer_pool, false);
  }
  ab = get_full_audio_buffer(producer_pool, false);
  while (ab != nullptr) {
    free(ab->buffer->bytes);
    free(ab->buffer);
    ab = get_full_audio_buffer(producer_pool, false);
  }
  free(producer_pool);
  producer_pool = nullptr;
}
} // namespace AudioOutput

extern "C" {
// callback from:
//   void __isr __time_critical_func(audio_i2s_dma_irq_handler)()
//   defined at my_pico_audio_i2s/audio_i2s.c
//   where i2s_callback_func() is declared with __attribute__((weak))
void i2s_callback_func() {
  if (running) {
    if (AudioOutput::buffer_callback) {
      AudioOutput::buffer_callback(producer_pool);
    }
  }
}
}
