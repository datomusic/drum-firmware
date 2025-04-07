
#ifndef BLOCK_H_WDR6BFX7
#define BLOCK_H_WDR6BFX7

#include "etl/array.h"
#include <stdint.h>

// This is essentially an std::array, but with an interface limited to what we
// actually use.
// There is no extra memory safety, and users are still required
// to avoid indexing out of bounds.

struct AudioBlock {
  int16_t &operator[](const size_t i) {
    // TODO: Add some kind of range checking, at least in debug?
    return data[i];
  }

  size_t size() const {
    return data.SIZE;
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
};

#endif /* end of include guard: BLOCK_H_WDR6BFX7 */
