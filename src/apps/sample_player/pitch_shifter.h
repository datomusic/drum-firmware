#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include "Arduino.h"
#include "AudioStream.h"

class PitchShifter : public AudioStream {
public:
  PitchShifter(void) : AudioStream(1, inputQueueArray) {
  }
  virtual void update(void);

private:
  audio_block_t *inputQueueArray[1];
};

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
