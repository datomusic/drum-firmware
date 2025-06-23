#include "audio_output.h"
#include "musin/hal/debug_utils.h" // For underrun counter
#include <algorithm>               // For std::clamp
#include <cmath>                   // For std::round

#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"

#include <optional>

#ifdef DATO_SUBMARINE
#include "musin/drivers/aic3204.hpp"
static musin::drivers::Aic3204 *codec_ptr = nullptr;
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

bool AudioOutput::init(musin::drivers::Aic3204 &codec) {
#ifdef DATO_SUBMARINE
  codec_ptr = &codec;
  // Set initial volume to 0dB (max)
  codec_ptr->set_dac_volume(0);
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
#ifdef DATO_SUBMARINE
  codec_ptr = nullptr;
#endif
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
  if (!codec_ptr) {
    return false;
  }
  // Clamp the input volume to the normalized range [0.0, 1.0]
  float clamped_volume = std::clamp(volume, 0.0f, 1.0f);

  // Apply a fast quadratic approximation of a square root curve (2x - x^2)
  // to provide more resolution at higher volumes without using sqrtf.
  float curved_volume = (2.0f * clamped_volume) - (clamped_volume * clamped_volume);

  // Map the curved value [0.0, 1.0] to the codec's dB range [-63.5dB, 0dB]
  // which corresponds to register values [-127, 0].
  float mapped_value = (curved_volume * 127.0f) - 127.0f;

  // Round to the nearest integer for the register value
  int8_t codec_register_value = static_cast<int8_t>(std::round(mapped_value));

  // Call the C++ driver method
  return codec_ptr->set_dac_volume(codec_register_value) == musin::drivers::Aic3204Status::OK;
#else
  // No codec defined, maybe control digital volume?
  // For now, just return true as there's nothing to set.
  (void)volume; // Mark as unused
  return true;
#endif
}

bool AudioOutput::route_line_in_to_headphone(bool enable) {
#ifdef DATO_SUBMARINE
  if (!codec_ptr) {
    return false;
  }
  return codec_ptr->route_in_to_headphone(enable) == musin::drivers::Aic3204Status::OK;
#else
  (void)enable;
  printf("AudioOutput Warning: Line-in routing unavailable (requires DATO_SUBMARINE build).\n");
  return false;
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
      return false; // Successfully processed a buffer
    } else {
      // Buffer was not available from the pool, potential underrun
      musin::hal::DebugUtils::g_audio_output_underruns++;
    }
  }

  return false; // No buffer processed or running is false
}
