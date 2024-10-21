#ifndef AUDIO_MEMORY_READER_H_R6GTYSPZ
#define AUDIO_MEMORY_READER_H_R6GTYSPZ

#include <stdint.h>

class AudioMemoryReader {
public:
  AudioMemoryReader() : encoding(0) {
  }

  void init(const unsigned int *sample_data) {
    this->data = sample_data;

    const uint32_t format = *this->data++;
    this->length = format & 0xFFFFFF;
    this->encoding = format >> 24;
    this->prior = 0;
    this->next = data;
  }

  void read_samples8(int16_t *buffer);
  bool has_data() {
    return this->encoding > 0;
  }

private:
  const unsigned int *data;
  const unsigned int *next;
  uint8_t encoding;
  uint8_t length;
  int16_t prior;
};

#endif /* end of include guard: AUDIO_MEMORY_READER_H_R6GTYSPZ */
