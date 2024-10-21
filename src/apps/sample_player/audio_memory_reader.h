#ifndef AUDIO_MEMORY_READER_H_R6GTYSPZ
#define AUDIO_MEMORY_READER_H_R6GTYSPZ

#include <stdint.h>

class AudioMemoryReader {
public:
  AudioMemoryReader() : encoding(0) {};

  void play(const unsigned int *data);
  bool has_data() {
    return this->encoding > 0;
  }

  void read_samples(int16_t *out, const uint16_t count);

private:
  const unsigned int *next;
  const unsigned int *beginning;
  uint32_t length;
  int16_t prior;
  volatile uint8_t encoding;
};

#endif /* end of include guard: AUDIO_MEMORY_READER_H_R6GTYSPZ */
