#ifndef INTRINSICS_H_Y4CZNOT3
#define INTRINSICS_H_Y4CZNOT3

#include <cstdint>

namespace intrinsics {
template <int bits> static constexpr int16_t signed_saturate(const int32_t value) {
  static_assert(bits == 16);

  if (value > INT16_MAX) {
    return INT16_MAX;
  }

  if (value < INT16_MIN) {
    return INT16_MIN;
  }

  return value;
}
} // namespace intrinsics
#endif /* end of include guard: INTRINSICS_H_Y4CZNOT3 */
