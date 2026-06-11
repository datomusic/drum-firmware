#ifndef MUSIN_FLASH_UF2_PARSER_H_QX3VN2LM
#define MUSIN_FLASH_UF2_PARSER_H_QX3VN2LM

#include <cstddef>
#include <cstdint>

#include "etl/array.h"
#include "etl/span.h"

namespace musin::flash {

// Incremental UF2 stream parser. Consumes arbitrary-size byte spans,
// validates each 512-byte UF2 block and emits its payload rebased from the
// link-time flash base to a target partition offset.
//
// Blocks carrying a different family (e.g. the RP2350-A2 erratum "absolute"
// block) or the NOT_MAIN_FLASH flag are skipped. Header-only with no SDK
// dependencies so it can be tested on the host.
class Uf2Parser {
public:
  static constexpr size_t BLOCK_SIZE = 512;
  static constexpr size_t PAYLOAD_SIZE = 256;

  static constexpr uint32_t MAGIC_START_0 = 0x0A324655;
  static constexpr uint32_t MAGIC_START_1 = 0x9E5D5157;
  static constexpr uint32_t MAGIC_END = 0x0AB16F30;

  static constexpr uint32_t FLAG_NOT_MAIN_FLASH = 0x00000001;
  static constexpr uint32_t FLAG_FAMILY_ID_PRESENT = 0x00002000;

  static constexpr uint32_t FAMILY_RP2350_ARM_S = 0xE48BFF59;
  static constexpr uint32_t FAMILY_ABSOLUTE = 0xE48BFF57;

  static constexpr uint32_t FLASH_BASE = 0x10000000;

  enum class Result {
    Ok,             // All consumed bytes processed without error
    Complete,       // Final block of the stream has been emitted
    BadMagic,       // Block framing magics did not match
    WrongFamily,    // No family ID present
    BadPayloadSize, // Flash block payload was not PAYLOAD_SIZE bytes
    OutOfRange,     // Target address outside the partition
    NonSequential,  // Block numbering was not consecutive
    EmitFailed,     // The emit callback reported failure (e.g. flash error)
  };

  struct Block {
    uint32_t flash_offset; // Rebased offset within flash (not XIP-mapped)
    etl::span<const uint8_t> payload;
    uint32_t block_no;
    uint32_t num_blocks;
  };

  // target_offset: flash offset of the partition the image is written to.
  // partition_size: capacity of that partition in bytes.
  constexpr Uf2Parser(uint32_t target_offset, uint32_t partition_size)
      : target_offset_(target_offset), partition_size_(partition_size) {
  }

  // Feeds bytes into the parser. `emit` is called once per accepted flash
  // block with a Block whose payload points into the parser's buffer; it must
  // return false to abort (reported as EmitFailed).
  // Returns Ok while more blocks are expected, Complete when the block with
  // number num_blocks-1 has been processed, or an error. After an error or
  // Complete the parser must be reset before reuse.
  template <typename Emit>
  constexpr Result push(etl::span<const uint8_t> bytes, Emit &&emit) {
    for (const uint8_t byte : bytes) {
      buffer_[buffer_pos_++] = byte;
      if (buffer_pos_ == BLOCK_SIZE) {
        buffer_pos_ = 0;
        const Result result = process_block(emit);
        if (result != Result::Ok) {
          return result;
        }
        if (complete_) {
          return Result::Complete;
        }
      }
    }
    return Result::Ok;
  }

  constexpr bool is_complete() const {
    return complete_;
  }

  // Number of payload bytes emitted so far.
  constexpr uint32_t bytes_emitted() const {
    return bytes_emitted_;
  }

  constexpr void reset() {
    buffer_pos_ = 0;
    next_block_no_ = 0;
    bytes_emitted_ = 0;
    complete_ = false;
  }

private:
  constexpr uint32_t read_u32(size_t offset) const {
    return static_cast<uint32_t>(buffer_[offset]) |
           (static_cast<uint32_t>(buffer_[offset + 1]) << 8) |
           (static_cast<uint32_t>(buffer_[offset + 2]) << 16) |
           (static_cast<uint32_t>(buffer_[offset + 3]) << 24);
  }

  template <typename Emit> constexpr Result process_block(Emit &emit) {
    if (read_u32(0) != MAGIC_START_0 || read_u32(4) != MAGIC_START_1 ||
        read_u32(BLOCK_SIZE - 4) != MAGIC_END) {
      return Result::BadMagic;
    }

    const uint32_t flags = read_u32(8);
    const uint32_t target_addr = read_u32(12);
    const uint32_t payload_size = read_u32(16);
    const uint32_t block_no = read_u32(20);
    const uint32_t num_blocks = read_u32(24);
    const uint32_t family_id = read_u32(28);

    if ((flags & FLAG_FAMILY_ID_PRESENT) == 0) {
      return Result::WrongFamily;
    }

    if (family_id != FAMILY_RP2350_ARM_S ||
        (flags & FLAG_NOT_MAIN_FLASH) != 0) {
      // Skip foreign-family blocks such as the RP2350-A2 erratum "absolute"
      // block at 0x10FFFF00. These carry their own block numbering, so they
      // do not participate in the sequence/completion tracking below.
      return Result::Ok;
    }

    if (block_no != next_block_no_ || block_no >= num_blocks) {
      return Result::NonSequential;
    }
    next_block_no_++;
    if (block_no + 1 == num_blocks) {
      complete_ = true;
    }

    if (payload_size != PAYLOAD_SIZE) {
      return Result::BadPayloadSize;
    }
    if (target_addr < FLASH_BASE ||
        target_addr + PAYLOAD_SIZE > FLASH_BASE + partition_size_) {
      return Result::OutOfRange;
    }

    const Block block{
        .flash_offset = target_addr - FLASH_BASE + target_offset_,
        .payload = etl::span<const uint8_t>{buffer_.data() + 32, PAYLOAD_SIZE},
        .block_no = block_no,
        .num_blocks = num_blocks,
    };
    if (!emit(block)) {
      return Result::EmitFailed;
    }
    bytes_emitted_ += PAYLOAD_SIZE;
    return Result::Ok;
  }

  uint32_t target_offset_;
  uint32_t partition_size_;
  etl::array<uint8_t, BLOCK_SIZE> buffer_{};
  size_t buffer_pos_ = 0;
  uint32_t next_block_no_ = 0;
  uint32_t bytes_emitted_ = 0;
  bool complete_ = false;
};

} // namespace musin::flash

#endif /* end of include guard: MUSIN_FLASH_UF2_PARSER_H_QX3VN2LM */
