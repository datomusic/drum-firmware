#include "audio_input.h"

#include "pico/audio_i2s.h"

#ifdef PICO_AUDIO_I2S_DATA_IN_PIN

#include "audio_i2s_rx.pio.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

static_assert(PICO_AUDIO_I2S_DATA_IN_PIN + 2 == PICO_AUDIO_I2S_CLOCK_PIN_BASE,
              "audio_i2s_rx.pio requires the data input pin two GPIOs below "
              "the clock pin base");

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)

namespace {

// Ring buffer the DMA writes into endlessly. Must be a power of two and
// aligned to its own size for the DMA address wrap to work.
constexpr size_t RING_WORDS = 512;
constexpr size_t RING_BYTES = RING_WORDS * sizeof(uint32_t);
constexpr uint32_t RING_SIZE_BITS = 11;
static_assert((1u << RING_SIZE_BITS) == RING_BYTES);

alignas(RING_BYTES) uint32_t ring_buffer[RING_WORDS];

int dma_channel = -1;
int rx_sm = -1;
uint program_offset = 0;
size_t read_index = 0;
bool running = false;

size_t __not_in_flash_func(dma_write_index)(int channel) {
  const uintptr_t write_addr = dma_channel_hw_addr(channel)->write_addr;
  return (write_addr - reinterpret_cast<uintptr_t>(ring_buffer)) /
         sizeof(uint32_t);
}

} // namespace

namespace musin::audio {

bool AudioInput::init() {
  if (running) {
    return true;
  }

  if (!pio_can_add_program(audio_pio, &audio_i2s_rx_program)) {
    return false;
  }
  program_offset = pio_add_program(audio_pio, &audio_i2s_rx_program);

  rx_sm = pio_claim_unused_sm(audio_pio, false);
  if (rx_sm < 0) {
    pio_remove_program(audio_pio, &audio_i2s_rx_program, program_offset);
    return false;
  }
  audio_i2s_rx_program_init(audio_pio, static_cast<uint>(rx_sm), program_offset,
                            PICO_AUDIO_I2S_DATA_IN_PIN);

  dma_channel = dma_claim_unused_channel(false);
  if (dma_channel < 0) {
    pio_sm_unclaim(audio_pio, static_cast<uint>(rx_sm));
    pio_remove_program(audio_pio, &audio_i2s_rx_program, program_offset);
    rx_sm = -1;
    return false;
  }

  dma_channel_config cfg = dma_channel_get_default_config(dma_channel);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
  channel_config_set_dreq(
      &cfg, pio_get_dreq(audio_pio, static_cast<uint>(rx_sm), false));
  channel_config_set_ring(&cfg, true, RING_SIZE_BITS);

  // Endless transfer mode (RP2350): the channel keeps writing into the ring
  // until aborted, so it never needs re-arming.
  constexpr uint32_t endless_count = DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS
                                     << DMA_CH0_TRANS_COUNT_MODE_LSB;
  dma_channel_configure(dma_channel, &cfg, ring_buffer, &audio_pio->rxf[rx_sm],
                        endless_count, true);

  read_index = 0;
  pio_sm_set_enabled(audio_pio, static_cast<uint>(rx_sm), true);

  running = true;
  return true;
}

void AudioInput::deinit() {
  if (!running) {
    return;
  }
  running = false;

  pio_sm_set_enabled(audio_pio, static_cast<uint>(rx_sm), false);
  dma_channel_abort(dma_channel);
  dma_channel_unclaim(dma_channel);
  dma_channel = -1;
  pio_sm_unclaim(audio_pio, static_cast<uint>(rx_sm));
  pio_remove_program(audio_pio, &audio_i2s_rx_program, program_offset);
  rx_sm = -1;
}

size_t __not_in_flash_func(AudioInput::read_samples)(AudioBlock &out) {
  // Snapshot the channel: deinit() can run concurrently on the main loop and
  // this is called from the I2S DMA interrupt.
  const int channel = dma_channel;
  if (!running || channel < 0) {
    return 0;
  }

  const size_t available =
      (dma_write_index(channel) - read_index) & (RING_WORDS - 1);
  const size_t to_read = available < out.size() ? available : out.size();

  for (size_t i = 0; i < to_read; ++i) {
    const uint32_t frame = ring_buffer[read_index];
    read_index = (read_index + 1) & (RING_WORDS - 1);

    const int32_t left = static_cast<int16_t>(frame >> 16);
    const int32_t right = static_cast<int16_t>(frame & 0xFFFF);
    out[i] = static_cast<int16_t>((left + right) >> 1);
  }
  return to_read;
}

} // namespace musin::audio

#else // !PICO_AUDIO_I2S_DATA_IN_PIN

namespace musin::audio {

bool AudioInput::init() {
  return false;
}

void AudioInput::deinit() {
}

size_t AudioInput::read_samples(AudioBlock &) {
  return 0;
}

} // namespace musin::audio

#endif // PICO_AUDIO_I2S_DATA_IN_PIN
