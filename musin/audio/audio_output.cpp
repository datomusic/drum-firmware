#include "audio_output.h"
#include "musin/hal/debug_utils.h" // For underrun counter
#include <algorithm>               // For std::clamp
#include <cmath>                   // For std::round

#include "hardware/irq.h"
#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"
#include "port/section_macros.h"

#include <optional>

#ifdef DATO_SUBMARINE
#include "musin/boards/dato_submarine.h" // For DATO_SUBMARINE_CODEC_RESET_PIN
#include "musin/drivers/aic3204.hpp"
#include "pico/time.h"
#include <etl/optional.h>

namespace {
// Encapsulate codec within this translation unit
etl::optional<musin::drivers::Aic3204> codec;

// Headphone jack detection state
constexpr uint32_t HEADPHONE_POLL_INTERVAL_MS = 20;     // Poll every 20ms
constexpr uint32_t HEADPHONE_DEBOUNCE_DURATION_MS = 75; // Require 75ms stable

// Debouncing state machine
std::optional<bool> current_debounced_state =
    std::nullopt;           // Cached debounced state
bool pending_state = false; // Current raw pin state
absolute_time_t pending_state_start_time =
    nil_time; // When pending_state was first observed
absolute_time_t last_poll_time = nil_time; // Last time we polled the codec

// Listener and policy
AudioOutput::HeadphoneListener headphone_listener = nullptr;
// Enabled by default to match the previous codec-managed behaviour
bool auto_speaker_mute_enabled = true;

} // namespace
#endif

static audio_buffer_pool_t *producer_pool;
static bool running = false;
static bool is_muted = false;
static BufferSource *fill_source = nullptr;

#define AUDIO_DMA_IRQ_NUM (DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ)

// Renders audio into every free producer buffer. Runs from the I2S DMA
// interrupt (after the i2s handler has freed the just-played buffer), so
// playback continues while the main loop is blocked. The entire call tree
// must be RAM-resident: during a flash erase, any flash access here would
// stall the interrupt and glitch the audio.
static void __not_in_flash_func(fill_buffers_from_irq)() {
  if (!running || fill_source == nullptr) {
    return;
  }
  audio_buffer_t *buffer;
  while ((buffer = take_audio_buffer(producer_pool, false)) != nullptr) {
    AudioBlock block;
    fill_source->fill_buffer(block);

    int16_t *out_samples = (int16_t *)buffer->buffer->bytes;
    for (size_t i = 0; i < block.size(); ++i) {
      out_samples[i] = block[i];
    }
    buffer->sample_count = block.size();
    give_audio_buffer(producer_pool, buffer);
  }
}

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define BUFFER_COUNT 3

static audio_format_t audio_format = {.sample_freq =
                                          AudioOutput::SAMPLE_FREQUENCY,
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

bool AudioOutput::init() {
#ifdef DATO_SUBMARINE
  codec.emplace(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, 100'000U,
                DATO_SUBMARINE_CODEC_RESET_PIN);

  if (!codec->is_initialized()) {
    return false; // Codec failed to initialize
  }

  // Set initial volume to 0dB (max)
  codec->set_dac_volume(0);
#endif

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

  return true;
}

void AudioOutput::deinit() {
#ifdef DATO_SUBMARINE
  if (codec) {
    codec->enter_sleep_mode();
  }
#endif
  running = false;

  audio_i2s_set_enabled(false);

  if (fill_source != nullptr) {
    irq_remove_handler(AUDIO_DMA_IRQ_NUM, fill_buffers_from_irq);
    fill_source = nullptr;
  }

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
  if (!codec) {
    return false;
  }

  // Scale float [0.0, 1.0] to integer [0, 1024] for high-precision fixed-point
  // math
  const int32_t input_volume =
      static_cast<int32_t>(std::clamp(volume, 0.0f, 1.0f) * 1024.0f);

  // --- Piecewise Linear Curve ---
  // All calculations are done in the [0, 1024] domain to avoid floats.
  const int32_t threshold = 512; // Breakpoint at 50% input (512/1024)
  const int32_t threshold_output =
      768; // At breakpoint, output is 75% (768/1024)

  int32_t curved_volume;
  if (input_volume <= threshold) {
    // Section 1: Low volume (gentle slope)
    // Maps input [0, 512] to output [0, 768]
    curved_volume = (input_volume * threshold_output) / threshold;
  } else {
    // Section 2: High volume (steep slope)
    // Maps input [512, 1024] to output [768, 1024]
    const int32_t remaining_input = input_volume - threshold;
    const int32_t remaining_output = 1024 - threshold_output;
    const int32_t input_range = 1024 - threshold;
    curved_volume =
        threshold_output + (remaining_input * remaining_output) / input_range;
  }

  // --- DAC Volume (Output Stage) ---
  // If volume is below 3% (31/1024), use hardware mute to prevent noise.
  bool dac_ok;
  if (curved_volume < 31) {
    dac_ok = codec->set_dac_muted(true) == musin::drivers::Aic3204Status::OK;
  } else {
    // Unmute and set volume: Maps [31, 1024] to DAC range [-63, 0]
    dac_ok = codec->set_dac_muted(false) == musin::drivers::Aic3204Status::OK;
    if (dac_ok) {
      int32_t mapped_dac_value = ((curved_volume - 31) * 63) / (1024 - 31);
      int8_t dac_register_value = static_cast<int8_t>(mapped_dac_value - 63);
      dac_ok = codec->set_dac_volume(dac_register_value) ==
               musin::drivers::Aic3204Status::OK;
    }
  }

  // --- Mixer Volume (Input Stage) ---
  // Map curved volume [0, 1024] to Mixer register value [-40, 0]
  int32_t mapped_mixer_value = (curved_volume * 40) / 1024;
  int8_t mixer_register_value = static_cast<int8_t>(mapped_mixer_value - 40);
  bool mixer_ok = codec->set_mixer_volume(mixer_register_value) ==
                  musin::drivers::Aic3204Status::OK;

  return dac_ok && mixer_ok;
#else
  // No codec defined, maybe control digital volume?
  // For now, just return true as there's nothing to set.
  (void)volume; // Mark as unused
  return true;
#endif
}

void AudioOutput::attach_source(BufferSource &source) {
  fill_source = &source;

  // The fill runs as a second shared handler on the I2S DMA interrupt,
  // after the i2s handler has freed the just-played buffer. Raise the
  // interrupt's priority so it can be kept enabled while all other
  // interrupts are masked during flash erase/program.
  irq_set_priority(AUDIO_DMA_IRQ_NUM, 0);
  irq_add_shared_handler(AUDIO_DMA_IRQ_NUM, fill_buffers_from_irq,
                         PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);

  // Prime the buffer pool so playback starts immediately.
  fill_buffers_from_irq();
}

void AudioOutput::update() {
#ifdef DATO_SUBMARINE
  // Time-based headphone detection polling with software debounce
  if (codec && time_reached(last_poll_time)) {
    last_poll_time = make_timeout_time_ms(HEADPHONE_POLL_INTERVAL_MS);

    auto raw_state_opt = codec->is_headphone_inserted();
    if (raw_state_opt) {
      bool raw_state = *raw_state_opt;

      // State machine: detect changes and debounce
      if (raw_state != pending_state) {
        // State changed, restart debounce timer
        pending_state = raw_state;
        pending_state_start_time = get_absolute_time();
      } else {
        // State is stable, check if debounce period elapsed
        int64_t elapsed_us = absolute_time_diff_us(pending_state_start_time,
                                                   get_absolute_time());
        if (elapsed_us >= (HEADPHONE_DEBOUNCE_DURATION_MS * 1000)) {
          // Debounce period elapsed with stable state
          if (!current_debounced_state ||
              (*current_debounced_state != pending_state)) {
            // State has changed after debouncing
            current_debounced_state = pending_state;

            // Notify listener if registered
            if (headphone_listener) {
              headphone_listener(pending_state);
            }

            // Apply auto-mute policy if enabled
            if (auto_speaker_mute_enabled && codec) {
              codec->set_amp_enabled(!pending_state);
            }
          }
        }
      }
    }
  }
#endif
}

bool AudioOutput::mute() {
#ifdef DATO_SUBMARINE
  if (!codec) {
    return false;
  }

  bool amp_ok =
      codec->set_amp_enabled(false) == musin::drivers::Aic3204Status::OK;
  bool headphone_ok =
      codec->set_headphone_enabled(false) == musin::drivers::Aic3204Status::OK;

  if (amp_ok && headphone_ok) {
    is_muted = true;
    return true;
  }
  return false;
#else
  // No codec defined, just track mute state
  is_muted = true;
  return true;
#endif
}

bool AudioOutput::unmute() {
#ifdef DATO_SUBMARINE
  if (!codec) {
    return false;
  }

  bool amp_ok =
      codec->set_amp_enabled(true) == musin::drivers::Aic3204Status::OK;
  bool headphone_ok =
      codec->set_headphone_enabled(true) == musin::drivers::Aic3204Status::OK;

  if (amp_ok && headphone_ok) {
    is_muted = false;
    return true;
  }
  return false;
#else
  // No codec defined, just track mute state
  is_muted = false;
  return true;
#endif
}

std::optional<bool> AudioOutput::headphones_inserted() {
#ifdef DATO_SUBMARINE
  return current_debounced_state;
#else
  return std::nullopt;
#endif
}

void AudioOutput::set_headphone_listener(HeadphoneListener listener) {
#ifdef DATO_SUBMARINE
  headphone_listener = listener;
#else
  (void)listener;
#endif
}

void AudioOutput::clear_headphone_listener() {
#ifdef DATO_SUBMARINE
  headphone_listener = nullptr;
#endif
}

void AudioOutput::enable_auto_speaker_mute(bool enable) {
#ifdef DATO_SUBMARINE
  auto_speaker_mute_enabled = enable;
#else
  (void)enable;
#endif
}
