#ifndef BUFFER_PLAYER_H_O1XVVMCG
#define BUFFER_PLAYER_H_O1XVVMCG

#include "audio_memory_reader.h"
#include <AudioStream.h>

class BufferPlayer : public AudioStream {
public:
  BufferPlayer(void) : AudioStream(0, NULL), playing(false) {
  }
  void play(const unsigned int *data);
  void stop(void);
  bool isPlaying(void);
  uint32_t positionMillis(void);
  uint32_t lengthMillis(void);
  virtual void update(void);

private:
  AudioMemoryReader reader;
  bool playing;
};

#endif /* end of include guard: BUFFER_PLAYER_H_O1XVVMCG */
