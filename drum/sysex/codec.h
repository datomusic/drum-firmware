#ifndef SYSEX_CODEC_H_JDGZXSHN
#define SYSEX_CODEC_H_JDGZXSHN

#include "etl/array.h"
#include "etl/iterator.h"
#include "etl/span.h"

namespace sysex::codec {

// Decodes legacy 7-bit SysEx bytes into 16bit values. 3 input bytes per single output value.
template <typename InputIt, typename OutputIt>
size_t decode_3_to_16bit(InputIt start, InputIt end, OutputIt out_start, OutputIt out_end) {
  size_t value_count = 0;
  size_t syx_count = 0;
  uint8_t tmp[2];
  auto out_it = out_start;

  while (start != end && out_it != out_end) {
    const uint8_t byte = (*start++);

    if (syx_count == 2) {
      syx_count = 0;
      *out_it++ = (tmp[0] << 14) | (tmp[1] << 7) | byte;
      value_count++;
    } else {
      tmp[syx_count] = byte;
      syx_count++;
    }
  }

  return value_count;
}

// Decodes a stream of 8-byte SysEx-safe groups into raw data bytes.
// `start` and `end` point to the encoded data.
// `output` is where the decoded data will be written.
// Returns the number of bytes written to `output`.
template <typename InputIt, typename OutputIt>
size_t decode_8_to_7(InputIt start, InputIt end, OutputIt output_start, OutputIt output_end) {
  size_t bytes_written = 0;
  auto out_it = output_start;

  while (static_cast<size_t>(etl::distance(start, end)) >= 8 && out_it != output_end) {
    const uint8_t msbs = *(start + 7);

    for (int i = 0; i < 7; ++i) {
      if (out_it == output_end) {
        break;
      }
      *out_it++ = (*(start + i)) | ((msbs >> i) & 0x01 ? 0x80 : 0x00);
      bytes_written++;
    }
    start += 8;
  }
  return bytes_written;
}

} // namespace sysex::codec

#endif /* end of include guard: SYSEX_CODEC_H_JDGZXSHN */

