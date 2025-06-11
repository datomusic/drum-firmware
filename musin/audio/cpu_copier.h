#pragma once
#include <algorithm> // For std::copy
#include <cstdint>

namespace musin {

struct CpuCopier {
  // init and deinit do nothing for the CPU implementation.
  static void init() {
  }
  static void deinit() {
  }

  // copy is now a static member function that uses std::copy.
  static void copy(int16_t *dest, const int16_t *src, size_t count) {
    std::copy(src, src + count, dest);
  }
};

} // namespace musin
