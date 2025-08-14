#ifndef SYSEX_CODEC_H_JDGZXSHN
#define SYSEX_CODEC_H_JDGZXSHN

#include "etl/iterator.h"

namespace sysex::codec {

// Decodes legacy 7-bit SysEx bytes into 16bit values. 3 input bytes per single
// output value.
template <typename InputIt, typename OutputIt>
size_t decode_3_to_16bit(InputIt start, InputIt end, OutputIt out_start,
                         OutputIt out_end) {
  size_t value_count = 0;
  size_t sysex_count = 0;
  uint8_t byte_buffer[2];
  auto out_it = out_start;

  while (start != end && out_it != out_end) {
    const uint8_t byte = (*start++);

    if (sysex_count == 2) {
      sysex_count = 0;
      *out_it++ = (byte_buffer[0] << 14) | (byte_buffer[1] << 7) | byte;
      value_count++;
    } else {
      byte_buffer[sysex_count] = byte;
      sysex_count++;
    }
  }

  return value_count;
}

// Decodes a stream of 8-byte SysEx-safe groups into raw data bytes.
// `start` and `end` point to the encoded data.
// `output_start` and `output_end` define the buffer for the decoded data.
// Returns a pair:
// - first: number of input bytes consumed
// - second: number of output bytes written
template <typename InputIt, typename OutputIt>
etl::pair<size_t, size_t> decode_8_to_7(InputIt start, InputIt end,
                                        OutputIt output_start,
                                        OutputIt output_end) {
  size_t bytes_written = 0;
  size_t bytes_read = 0;
  auto out_it = output_start;

  while (static_cast<size_t>(etl::distance(start, end)) >= 8 &&
         out_it != output_end) {
    const uint8_t msbs = *(start + 7);
    const size_t remaining_out_space = etl::distance(out_it, output_end);
    const size_t bytes_to_process = etl::min(static_cast<size_t>(7), remaining_out_space);

    for (size_t i = 0; i < bytes_to_process; ++i) {
      *out_it++ = (*(start + i)) | ((msbs >> i) & 0x01 ? 0x80 : 0x00);
    }
    bytes_written += bytes_to_process;
    
    // Only advance the input iterator by the full 8 bytes if we processed all 7 data bytes
    if (bytes_to_process == 7) {
        start += 8;
        bytes_read += 8;
    } else {
        // If we didn't have enough output space for all 7 bytes, we can't consume the input block.
        break;
    }
  }
  return {bytes_read, bytes_written};
}

} // namespace sysex::codec

#endif /* end of include guard: SYSEX_CODEC_H_JDGZXSHN */
