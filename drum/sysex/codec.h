#ifndef SYEX_CODEC_H_JDGZXSHN
#define SYEX_CODEC_H_JDGZXSHN

#include "etl/array.h"

namespace sysex::codec {

// Decodes 7-bit SysEx bytes into 16bit values. 3 input bytes per single output value.
// Note: We're wasting some memory by requiring output of the same size as input, but that's easiest
//       for now.
template <int MaxBytes>
constexpr size_t decode(const typename etl::array<uint8_t, MaxBytes>::const_iterator start,
                        const typename etl::array<uint8_t, MaxBytes>::const_iterator end,
                        etl::array<uint16_t, MaxBytes> &output) {

  size_t value_count = 0;
  size_t syx_count = 0;

  uint8_t tmp[2];

  auto iterator = start;

  while (iterator != end) {
    const uint8_t byte = (*iterator++);

    if (syx_count == 2) {
      syx_count = 0;
      output[value_count] = (tmp[0] << 14) + (tmp[1] << 7) + byte;
      value_count++;
    } else {
      tmp[syx_count] = byte;
      syx_count++;
    }
  }

  return value_count;
}

// TODO: Add encode when we need it

} // namespace sysex::codec

#endif /* end of include guard: SYEX_CODEC_H_JDGZXSHN */
