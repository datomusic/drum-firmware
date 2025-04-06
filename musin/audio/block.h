
#ifndef BLOCK_H_WDR6BFX7
#define BLOCK_H_WDR6BFX7

#include "etl/array.h"
#include <stdint.h>

// This is essentially an std::array, but with the ability to communicate being parially filled.
// There is no extra memory safety, and users are still required to avoid indexing out of bounds.

struct AudioBlock {
  int16_t &operator[](const size_t i) {
    // TODO: Add some kind of range checking, at least in debug?
    return data[i];
  }

  size_t size() const {
    return current_size;
  }

  void resize(size_t new_size) {
    // TODO: Should emit warming, or panic?
    //       This indicates incorrect usage.
    if (new_size >= data.SIZE) {
      new_size = data.SIZE;
    }

    current_size = new_size;
  }

  int16_t *begin() {
    return data.begin();
  }

  int16_t *end() {
    return begin() + size();
  }

  const int16_t *cbegin() const {
    return data.cbegin();
  }

  const int16_t *cend() const {
    return cbegin() + size();
  }

private:
  etl::array<int16_t, AUDIO_BLOCK_SAMPLES> data;
  size_t current_size = 0;
};

#endif /* end of include guard: BLOCK_H_WDR6BFX7 */
