#include "buffer_player.h"
#include "utility/dspinst.h"

void BufferPlayer::play(const unsigned int *data) {
  reader.play(data);
}

void BufferPlayer::update(void) {
  if (!reader.has_data()) {
    return;
  }

  audio_block_t *block = allocate();
  if (block == NULL) {
    return;
  }

  reader.read_samples(block->data, AUDIO_BLOCK_SAMPLES);
  transmit(block);
  release(block);
}
