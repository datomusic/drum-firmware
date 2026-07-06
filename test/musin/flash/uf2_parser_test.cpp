#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "etl/span.h"
#include "etl/vector.h"

#include "musin/flash/uf2_parser.h"

using musin::flash::Uf2Parser;
using Result = Uf2Parser::Result;

namespace {

constexpr uint32_t PARTITION_OFFSET = 0x100000; // Firmware B
constexpr uint32_t PARTITION_SIZE = 0x100000;   // 1 MiB

void write_u32(std::vector<uint8_t> &out, size_t offset, uint32_t value) {
  out[offset] = value & 0xFF;
  out[offset + 1] = (value >> 8) & 0xFF;
  out[offset + 2] = (value >> 16) & 0xFF;
  out[offset + 3] = (value >> 24) & 0xFF;
}

std::vector<uint8_t>
make_block(uint32_t block_no, uint32_t num_blocks, uint32_t target_addr,
           uint32_t family = Uf2Parser::FAMILY_RP2350_ARM_S,
           uint32_t flags = Uf2Parser::FLAG_FAMILY_ID_PRESENT,
           uint32_t payload_size = Uf2Parser::PAYLOAD_SIZE) {
  std::vector<uint8_t> block(Uf2Parser::BLOCK_SIZE, 0);
  write_u32(block, 0, Uf2Parser::MAGIC_START_0);
  write_u32(block, 4, Uf2Parser::MAGIC_START_1);
  write_u32(block, 8, flags);
  write_u32(block, 12, target_addr);
  write_u32(block, 16, payload_size);
  write_u32(block, 20, block_no);
  write_u32(block, 24, num_blocks);
  write_u32(block, 28, family);
  for (size_t i = 0; i < Uf2Parser::PAYLOAD_SIZE; ++i) {
    block[32 + i] = static_cast<uint8_t>((block_no + i) & 0xFF);
  }
  write_u32(block, Uf2Parser::BLOCK_SIZE - 4, Uf2Parser::MAGIC_END);
  return block;
}

struct EmittedBlock {
  uint32_t flash_offset;
  uint8_t first_byte;
};

struct Collector {
  std::vector<EmittedBlock> blocks;
  bool accept = true;

  bool operator()(const Uf2Parser::Block &block) {
    blocks.push_back({block.flash_offset, block.payload[0]});
    return accept;
  }
};

} // namespace

TEST_CASE("Uf2Parser accepts a well-formed stream and rebases addresses") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;

  auto block0 = make_block(0, 2, 0x10000000);
  auto block1 = make_block(1, 2, 0x10000100);

  REQUIRE(parser.push(etl::span<const uint8_t>{block0.data(), block0.size()},
                      collector) == Result::Ok);
  REQUIRE_FALSE(parser.is_complete());
  REQUIRE(parser.push(etl::span<const uint8_t>{block1.data(), block1.size()},
                      collector) == Result::Complete);
  REQUIRE(parser.is_complete());

  REQUIRE(collector.blocks.size() == 2);
  REQUIRE(collector.blocks[0].flash_offset == PARTITION_OFFSET);
  REQUIRE(collector.blocks[1].flash_offset == PARTITION_OFFSET + 0x100);
  REQUIRE(parser.bytes_emitted() == 2 * Uf2Parser::PAYLOAD_SIZE);
}

TEST_CASE("Uf2Parser handles arbitrary chunk boundaries") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;

  std::vector<uint8_t> stream = make_block(0, 2, 0x10000000);
  auto block1 = make_block(1, 2, 0x10000100);
  stream.insert(stream.end(), block1.begin(), block1.end());

  // Feed in uneven slices that straddle block boundaries.
  size_t pos = 0;
  Result last = Result::Ok;
  const size_t slice_sizes[] = {1, 7, 300, 511, 200, 5};
  size_t slice_index = 0;
  while (pos < stream.size()) {
    const size_t n =
        std::min(slice_sizes[slice_index % 6], stream.size() - pos);
    slice_index++;
    last = parser.push(etl::span<const uint8_t>{stream.data() + pos, n},
                       collector);
    REQUIRE((last == Result::Ok || last == Result::Complete));
    pos += n;
  }
  REQUIRE(last == Result::Complete);
  REQUIRE(collector.blocks.size() == 2);
}

TEST_CASE("Uf2Parser skips the erratum absolute block") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;

  // picotool places the absolute-family erratum block (targeting 0x10FFFF00,
  // outside any partition) first in the stream, with its OWN block numbering;
  // the main image's numbering restarts at 0 afterwards.
  auto abs_block = make_block(0, 2, 0x10FFFF00, Uf2Parser::FAMILY_ABSOLUTE);
  auto fw_block = make_block(0, 1, 0x10000000);

  REQUIRE(
      parser.push(etl::span<const uint8_t>{abs_block.data(), abs_block.size()},
                  collector) == Result::Ok);
  REQUIRE(collector.blocks.empty());
  REQUIRE(
      parser.push(etl::span<const uint8_t>{fw_block.data(), fw_block.size()},
                  collector) == Result::Complete);
  REQUIRE(collector.blocks.size() == 1);
  REQUIRE(parser.bytes_emitted() == Uf2Parser::PAYLOAD_SIZE);
}

TEST_CASE("Uf2Parser rejects bad magics") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  auto block = make_block(0, 1, 0x10000000);
  block[0] ^= 0xFF;
  REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                      collector) == Result::BadMagic);
}

TEST_CASE("Uf2Parser rejects blocks without family ID") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  auto block = make_block(0, 1, 0x10000000, Uf2Parser::FAMILY_RP2350_ARM_S, 0);
  REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                      collector) == Result::WrongFamily);
}

TEST_CASE("Uf2Parser skips not-main-flash blocks") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  auto block = make_block(0, 2, 0x10000000, Uf2Parser::FAMILY_RP2350_ARM_S,
                          Uf2Parser::FLAG_FAMILY_ID_PRESENT |
                              Uf2Parser::FLAG_NOT_MAIN_FLASH);
  REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                      collector) == Result::Ok);
  REQUIRE(collector.blocks.empty());
}

TEST_CASE("Uf2Parser rejects out-of-range targets") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;

  SECTION("below flash base") {
    auto block = make_block(0, 1, 0x0FFFFF00);
    REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                        collector) == Result::OutOfRange);
  }
  SECTION("beyond partition size") {
    auto block = make_block(0, 1, 0x10000000 + PARTITION_SIZE);
    REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                        collector) == Result::OutOfRange);
  }
  SECTION("straddling the partition end") {
    auto block = make_block(0, 1, 0x10000000 + PARTITION_SIZE - 0x80);
    REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                        collector) == Result::OutOfRange);
  }
  SECTION("target address near uint32_t overflow") {
    auto block = make_block(0, 1, 0xFFFFFF00);
    REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                        collector) == Result::OutOfRange);
  }
}

TEST_CASE("Uf2Parser rejects non-sequential block numbers") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  auto block0 = make_block(0, 3, 0x10000000);
  auto block2 = make_block(2, 3, 0x10000200);
  REQUIRE(parser.push(etl::span<const uint8_t>{block0.data(), block0.size()},
                      collector) == Result::Ok);
  REQUIRE(parser.push(etl::span<const uint8_t>{block2.data(), block2.size()},
                      collector) == Result::NonSequential);
}

TEST_CASE("Uf2Parser rejects unexpected payload sizes") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  auto block = make_block(0, 1, 0x10000000, Uf2Parser::FAMILY_RP2350_ARM_S,
                          Uf2Parser::FLAG_FAMILY_ID_PRESENT, 128);
  REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                      collector) == Result::BadPayloadSize);
}

TEST_CASE("Uf2Parser reports emit failure") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  collector.accept = false;
  auto block = make_block(0, 1, 0x10000000);
  REQUIRE(parser.push(etl::span<const uint8_t>{block.data(), block.size()},
                      collector) == Result::EmitFailed);
}

TEST_CASE("Uf2Parser reset allows reuse after error") {
  Uf2Parser parser(PARTITION_OFFSET, PARTITION_SIZE);
  Collector collector;
  auto bad = make_block(0, 1, 0x10000000);
  bad[0] ^= 0xFF;
  REQUIRE(parser.push(etl::span<const uint8_t>{bad.data(), bad.size()},
                      collector) == Result::BadMagic);

  parser.reset();
  auto good = make_block(0, 1, 0x10000000);
  REQUIRE(parser.push(etl::span<const uint8_t>{good.data(), good.size()},
                      collector) == Result::Complete);
  REQUIRE(parser.bytes_emitted() == Uf2Parser::PAYLOAD_SIZE);
}
