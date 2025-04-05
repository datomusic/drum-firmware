
#ifndef BLOCK_H_WDR6BFX7
#define BLOCK_H_WDR6BFX7

#include "etl/array.h"
#include <stdint.h>

struct AudioBlock {
  int16_t &operator[](const size_t i) {
    return data[i];
  }

  size_t size() const {
    return current_size;
  }

  void resize(size_t new_size) {
    if (new_size >= data.SIZE) {
      new_size = data.SIZE;
    }

    current_size = new_size;
  }

  int16_t *begin() {
    return data.begin();
  }

private:
  etl::array<int16_t, AUDIO_BLOCK_SAMPLES> data;
  size_t current_size = 0;
};

#endif /* end of include guard: BLOCK_H_WDR6BFX7 */
