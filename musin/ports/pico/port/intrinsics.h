#ifndef INTRINSICS_H_KIJFC2HZ
#define INTRINSICS_H_KIJFC2HZ

#include <arm_acle.h>
#include <cstdint>

namespace intrinsics {
template <int bits> static constexpr int16_t signed_saturate(const int32_t value) {
  return __ssat(value, bits);
}
} // namespace intrinsics

#endif /* end of include guard: INTRINSICS_H_KIJFC2HZ */
