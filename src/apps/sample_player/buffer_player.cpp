#include "buffer_player.h"
#include "utility/dspinst.h"

void BufferPlayer::play(const unsigned int *data) {
  reader.play(data);
}

void BufferPlayer::update(void) {
  audio_block_t *block;

  if (!reader.is_playing()) {
    return;
  }

  block = allocate();
  if (block == NULL) {
    return;
  }

  const uint32_t consumed =
      reader.read_samples(block->data, AUDIO_BLOCK_SAMPLES);
  if (consumed > 0) {
    transmit(block);
  }

  release(block);
}
