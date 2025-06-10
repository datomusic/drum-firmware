#pragma once
#include <algorithm> // For std::copy
#include <cstdint>

namespace musin {

struct CpuCopier {
  // copy is a non-static member function that uses std::copy.
  void copy(int16_t *dest, const int16_t *src, size_t count) const {
    std::copy(src, src + count, dest);
  }
  // Constructor and destructor are empty and will be optimized away.
};

} // namespace musin
