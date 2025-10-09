#ifndef SYSEX_UF2_VALIDATOR_H_
#define SYSEX_UF2_VALIDATOR_H_

#include "etl/array.h"
#include <cstdint>

struct uf2_block {
  uint32_t magic_start0;
  uint32_t magic_start1;
  uint32_t flags;
  uint32_t target_addr;
  uint32_t payload_size;
  uint32_t block_no;
  uint32_t num_blocks;
  uint32_t file_size;
  uint8_t  data[476];
  uint32_t magic_end;
};

constexpr uint32_t UF2_MAGIC_START0 = 0x0A324655u;
constexpr uint32_t UF2_MAGIC_START1 = 0x9E5D5157u;
constexpr uint32_t UF2_MAGIC_END    = 0x0AB16F30u;
constexpr uint32_t UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000u;
constexpr uint32_t ABSOLUTE_FAMILY_ID = 0xe48bff57u;
constexpr uint32_t RP2350_ARM_S_FAMILY_ID = 0xe48bff59u;

namespace sysex {

struct UF2BlockValidator {
  static constexpr size_t MAX_BLOCKS = 4096;

  constexpr UF2BlockValidator() : num_blocks_expected(0), blocks_received_count(0) {
    blocks_received.fill(false);
  }

  constexpr void reset() {
    blocks_received.fill(false);
    num_blocks_expected = 0;
    blocks_received_count = 0;
  }

  enum class ValidationResult {
    Success,
    InvalidMagicStart0,
    InvalidMagicStart1,
    InvalidMagicEnd,
    PayloadSizeExceeded,
    InconsistentBlockCount,
    BlockNumberOutOfBounds,
    DuplicateBlock,
    InvalidFamilyID,
    TooManyBlocks,
  };

  constexpr ValidationResult validate_block(const uf2_block& block) {
    if (block.magic_start0 != UF2_MAGIC_START0) {
      return ValidationResult::InvalidMagicStart0;
    }
    if (block.magic_start1 != UF2_MAGIC_START1) {
      return ValidationResult::InvalidMagicStart1;
    }
    if (block.magic_end != UF2_MAGIC_END) {
      return ValidationResult::InvalidMagicEnd;
    }
    if (block.payload_size > 476) {
      return ValidationResult::PayloadSizeExceeded;
    }

    if (block.num_blocks > MAX_BLOCKS) {
      return ValidationResult::TooManyBlocks;
    }

    if (block.block_no == 0) {
      // This indicates the start of a new UF2 image within the stream.
      // We reset the block tracking state.
      num_blocks_expected = block.num_blocks;
      blocks_received.fill(false);
      blocks_received_count = 0;
    } else {
      // For subsequent blocks, we don't check num_blocks again.
      // This handles UF2 files with special block structures (e.g., from --abs-block)
      // where the num_blocks field might not be consistent.
    }

    if (block.block_no >= num_blocks_expected) {
      return ValidationResult::BlockNumberOutOfBounds;
    }

    if (blocks_received[block.block_no]) {
      return ValidationResult::DuplicateBlock;
    }

    if (block.flags & UF2_FLAG_FAMILY_ID_PRESENT) {
      if (block.file_size != ABSOLUTE_FAMILY_ID &&
          block.file_size != RP2350_ARM_S_FAMILY_ID) {
        return ValidationResult::InvalidFamilyID;
      }
    }

    blocks_received[block.block_no] = true;
    blocks_received_count++;

    return ValidationResult::Success;
  }

  constexpr bool all_blocks_received() const {
    return num_blocks_expected > 0 && blocks_received_count == num_blocks_expected;
  }

  constexpr uint32_t get_expected_blocks() const {
    return num_blocks_expected;
  }

  constexpr uint32_t get_received_count() const {
    return blocks_received_count;
  }

private:
  etl::array<bool, MAX_BLOCKS> blocks_received;
  uint32_t num_blocks_expected;
  uint32_t blocks_received_count;
};

} // namespace sysex

#endif
