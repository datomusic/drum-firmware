#ifndef AUDIO_MEMORY_READER_H_R6GTYSPZ
#define AUDIO_MEMORY_READER_H_R6GTYSPZ

#include <stdint.h>

struct AudioMemoryReader {
  AudioMemoryReader() : encoding(0) {};

  // Reader interface
  void init(const unsigned int *data, const uint32_t data_length);

  // Reader interface
  bool has_data() {
    return this->encoding > 0;
  }

  // Reader interface
  void read_samples(int16_t *out, const uint16_t count);

private:
  int read_next(){
    const unsigned int* end = this->beginning + this->data_length;
    if (next == end) {
      encoding = 0;
      return 0;
    }else{
      const int sample = *next;
      ++next;
      return sample;
    }
  }

  const unsigned int *next;
  const unsigned int *beginning;
  uint32_t length;
  int16_t prior;
  volatile uint8_t encoding;
  uint32_t data_length;
};

#endif /* end of include guard: AUDIO_MEMORY_READER_H_R6GTYSPZ */
