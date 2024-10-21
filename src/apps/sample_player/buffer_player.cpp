#include "buffer_player.h"
#include "utility/dspinst.h"

void BufferPlayer::play(const unsigned int *data) {
  this->reader.init(data);
  this->playing = true;
}

void BufferPlayer::stop(void) {
  this->playing = false;
}

void BufferPlayer::update(void) {
  audio_block_t *block;
  int16_t *out;
  int i;

  if (!playing) {
    return;
  }

  block = allocate();
  if (block == NULL) {
    return;
  }

  out = block->data;

  for (i = 0; i < AUDIO_BLOCK_SAMPLES; i += 8) {
    if (reader.has_data()) {
      reader.read_samples8(out);
    }
  }

  transmit(block);
  release(block);

  if (!reader.has_data()) {
    this->playing = false;
  }
}

/*
#define B2M_88200                                                              \
  (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT / 2.0)
#define B2M_44100                                                              \
  (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT) // 97352592
#define B2M_22050                                                              \
  (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 2.0)
#define B2M_11025                                                              \
  (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 4.0)

uint32_t BufferPlayer::positionMillis(void) {
  uint8_t p;
  const uint8_t *n, *b;
  uint32_t b2m;

  __disable_irq();
  p = playing;
  n = (const uint8_t *)next;
  b = (const uint8_t *)beginning;
  __enable_irq();
  switch (p) {
  case 0x81: // 16 bit PCM, 44100 Hz
    b2m = B2M_88200;
    break;
  case 0x01: // u-law encoded, 44100 Hz
  case 0x82: // 16 bits PCM, 22050 Hz
    b2m = B2M_44100;
    break;
  case 0x02: // u-law encoded, 22050 Hz
  case 0x83: // 16 bit PCM, 11025 Hz
    b2m = B2M_22050;
    break;
  case 0x03: // u-law encoded, 11025 Hz
    b2m = B2M_11025;
    break;
  default:
    return 0;
  }
  if (p == 0)
    return 0;
  return ((uint64_t)(n - b) * b2m) >> 32;
}

uint32_t BufferPlayer::lengthMillis(void) {
  uint8_t p;
  const uint32_t *b;
  uint32_t b2m;

  __disable_irq();
  p = playing;
  b = (const uint32_t *)beginning;
  __enable_irq();
  switch (p) {
  case 0x81: // 16 bit PCM, 44100 Hz
  case 0x01: // u-law encoded, 44100 Hz
    b2m = B2M_44100;
    break;
  case 0x82: // 16 bits PCM, 22050 Hz
  case 0x02: // u-law encoded, 22050 Hz
    b2m = B2M_22050;
    break;
  case 0x83: // 16 bit PCM, 11025 Hz
  case 0x03: // u-law encoded, 11025 Hz
    b2m = B2M_11025;
    break;
  default:
    return 0;
  }
  return ((uint64_t)(*(b - 1) & 0xFFFFFF) * b2m) >> 32;
}
*/
