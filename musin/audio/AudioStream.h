#ifndef AUDIOSTREAM_H_LJR8VRG0
#define AUDIOSTREAM_H_LJR8VRG0

#include <stdint.h>

struct audio_block_t {
  int16_t data[AUDIO_BLOCK_SAMPLES];
};

class AudioStream {
public:
  AudioStream(unsigned char ninput, audio_block_t **iqueue) {
  }

protected:
  static void release(audio_block_t *block);
  audio_block_t *receiveReadOnly(unsigned int index = 0);
  audio_block_t *receiveWritable(unsigned int index = 0);
  void transmit(audio_block_t *block, unsigned char index = 0);

  static audio_block_t *allocate(void);

private:
  virtual void update(void) = 0;
};

#endif /* end of include guard: AUDIOSTREAM_H_LJR8VRG0 */
