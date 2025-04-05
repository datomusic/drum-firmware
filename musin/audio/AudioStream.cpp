#include "AudioStream.h"

audio_block_t *AudioStream::allocate(void) {
  return nullptr;
}

void AudioStream::release(audio_block_t *block) {
}

void AudioStream::transmit(audio_block_t *block, unsigned char index) {
}

audio_block_t *AudioStream::receiveReadOnly(unsigned int index) {
}

audio_block_t *AudioStream::receiveWritable(unsigned int index) {
}
