#ifndef BUFFER_PLAYER_H_O1XVVMCG
#define BUFFER_PLAYER_H_O1XVVMCG

#include "audio_memory_reader.h"
#include <AudioStream.h>

template <typename Reader> struct BufferPlayer : public AudioStream {
  BufferPlayer(Reader reader) : AudioStream(0, NULL), reader(reader) {
  }

  void play() {
    reader.reset();
  }

  virtual void update() {
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

  Reader reader;
};

#endif /* end of include guard: BUFFER_PLAYER_H_O1XVVMCG */
