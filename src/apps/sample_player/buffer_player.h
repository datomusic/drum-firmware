#ifndef BUFFER_PLAYER_H_O1XVVMCG
#define BUFFER_PLAYER_H_O1XVVMCG

#include "audio_memory_reader.h"
#include <AudioStream.h>

class BufferPlayer : public AudioStream {

public:
  BufferPlayer(void) : AudioStream(0, NULL) {
  }
  void play(const unsigned int *data);
  virtual void update(void);

private:
  AudioMemoryReader reader;
};

#endif /* end of include guard: BUFFER_PLAYER_H_O1XVVMCG */
