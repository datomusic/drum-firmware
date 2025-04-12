#include "audio_output.h"
#include <algorithm> // For std::clamp
#include <cmath>     // For std::round

#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"

#ifdef DATO_SUBMARINE
#include "musin/drivers/aic3204.h"
#endif

static audio_buffer_pool_t *producer_pool;
static bool running = false;

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define BUFFER_COUNT 3

static audio_format_t audio_format = {.sample_freq = AudioOutput::SAMPLE_FREQUENCY,
                                      .format = AUDIO_BUFFER_FORMAT_PCM_S16,
                                      .channel_count = 1};

static audio_buffer_format_t producer_format = {.format = &audio_format, .sample_stride = 2};

struct audio_i2s_config i2s_config = {
    .data_pin = PICO_AUDIO_I2S_DATA_PIN,
    .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
    .dma_channel = 0,
    .pio_sm = 0,
};

bool AudioOutput::init() {
#ifdef DATO_SUBMARINE
  // Initialize AIC3204 codec with I2C0 pins (GP0=SDA, GP1=SCL) at 400kHz
  printf("Initializing AIC3204 codec\n");
  if (!aic3204_init(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, 100'000U)) {
    printf("Failed to initialize AIC3204 codec\n");
    return false;
  }
  // Set initial volume to 0dB (max)
  aic3204_dac_set_volume(0);
#endif

  audio_format.sample_freq = SAMPLE_FREQUENCY;

  producer_pool = audio_new_producer_pool(&producer_format, BUFFER_COUNT, AUDIO_BLOCK_SAMPLES);

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

  return true;
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

bool AudioOutput::volume(float volume) {
#ifdef DATO_SUBMARINE
  // Map [0.0, ~1.378] to the AIC3204 range [-127, 48]
  // 0.0f -> -127 (-63.5 dB)
  // 1.0f -> 0 (0 dB)
  // ~1.378f -> 48 (+24 dB)
  // Use a linear mapping based on the 0dB point (1.0f -> 0)
  // Slope below 1.0 = 127 steps / 1.0f = 127
  // Slope above 1.0 = 48 steps / (max_vol - 1.0f). Let's use the same slope for simplicity.
  // So, mapped_value = (volume - 1.0f) * 127
  float mapped_value = (volume - 1.0f) * 127.0f;

  // Clamp to the codec's hardware limits [-127, 48]
  mapped_value = std::clamp(mapped_value, -127.0f, 48.0f);

  // Round to the nearest integer for the register value
  int8_t codec_register_value = static_cast<int8_t>(std::round(mapped_value));

  // Call the underlying C driver function
  return aic3204_dac_set_volume(codec_register_value);
#else
  // No codec defined, maybe control digital volume?
  // For now, just return true as there's nothing to set.
  (void)volume; // Mark as unused
  return true;
#endif
}

bool AudioOutput::update(BufferSource &source) {
  if (running) {
    audio_buffer_t *buffer = take_audio_buffer(producer_pool, false);
    if (buffer != nullptr) {
      // printf("GOT BUFFER\n");

      AudioBlock block;
      source.fill_buffer(block);

      // Copy mono samples directly to the stereo buffer
      // The pico-sdk audio layer expects stereo, but we configure I2S for mono input.
      // It seems to handle the duplication internally or expects mono data in the buffer.
      // Let's copy directly for now. If stereo is needed, duplicate samples here.
      // NOTE: The previous digital volume scaling `(volume * block[i]) >> 8u` is removed.
      // Volume is now controlled solely by the hardware codec via AudioOutput::volume().
      int16_t *out_samples = (int16_t *)buffer->buffer->bytes;
      for (size_t i = 0; i < block.size(); ++i) {
        out_samples[i] = block[i];
        // If true stereo output needed:
        // out_samples[i*2 + 0] = block[i]; // Left
        // out_samples[i*2 + 1] = block[i]; // Right (duplicate mono)
      }

      buffer->sample_count = block.size(); // Should match AUDIO_BLOCK_SAMPLES

      give_audio_buffer(producer_pool, buffer);
      return false;
    }
  }

  return false;
}
