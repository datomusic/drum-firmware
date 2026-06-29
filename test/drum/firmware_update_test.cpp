#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "etl/span.h"
#include "etl/vector.h"

#include "musin/hal/null_logger.h"

#include "drum/sysex/firmware_update.h"

absolute_time_t mock_current_time = 0;

namespace {

struct FakeWriter {
  bool begin_ok = true;
  bool write_ok = true;
  bool finalize_ok = true;

  uint32_t begun_size = 0;
  std::vector<uint8_t> begun_sha;
  std::vector<uint8_t> written;
  int abort_count = 0;
  int finalize_count = 0;

  bool begin(uint32_t total_size, etl::span<const uint8_t> sha256) {
    begun_size = total_size;
    begun_sha.assign(sha256.begin(), sha256.end());
    written.clear();
    return begin_ok;
  }

  bool write(etl::span<const uint8_t> bytes) {
    if (!write_ok) {
      return false;
    }
    written.insert(written.end(), bytes.begin(), bytes.end());
    return true;
  }

  bool finalize() {
    finalize_count++;
    return finalize_ok;
  }

  void abort() {
    abort_count++;
  }
};

using Update = sysex::FirmwareUpdate<FakeWriter>;
using Result = Update::Result;
using Tag = Update::Tag;

struct MockSender {
  etl::vector<uint8_t, 16> sent;
  void operator()(uint8_t tag) {
    sent.push_back(tag);
  }
  uint8_t last() const {
    return sent.back();
  }
};

const std::vector<uint8_t> HEADER = {0x00, 0x22, 0x01, 0x65};

std::vector<uint8_t> encode_8_to_7(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i < data.size(); i += 7) {
    uint8_t msbs = 0;
    size_t group = std::min<size_t>(7, data.size() - i);
    // The decoder consumes full 8-byte groups only; pad the final group.
    for (size_t j = 0; j < 7; ++j) {
      const uint8_t byte = (j < group) ? data[i + j] : 0;
      out.push_back(byte & 0x7F);
      msbs |= static_cast<uint8_t>((byte >> 7) & 0x01) << j;
    }
    out.push_back(msbs);
  }
  return out;
}

std::vector<uint8_t> encode_3_to_16bit(const std::vector<uint8_t> &bytes) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i < bytes.size(); i += 2) {
    const uint16_t value =
        static_cast<uint16_t>(bytes[i]) |
        (i + 1 < bytes.size() ? static_cast<uint16_t>(bytes[i + 1]) << 8 : 0);
    out.push_back((value >> 14) & 0x7F);
    out.push_back((value >> 7) & 0x7F);
    out.push_back(value & 0x7F);
  }
  return out;
}

std::vector<uint8_t> make_begin_chunk(uint32_t total_size,
                                      uint8_t sha_fill = 0xAB) {
  std::vector<uint8_t> payload;
  payload.push_back(total_size & 0xFF);
  payload.push_back((total_size >> 8) & 0xFF);
  payload.push_back((total_size >> 16) & 0xFF);
  payload.push_back((total_size >> 24) & 0xFF);
  for (int i = 0; i < 32; ++i) {
    payload.push_back(sha_fill);
  }
  payload.push_back(1); // version major
  payload.push_back(2); // minor
  payload.push_back(3); // patch

  std::vector<uint8_t> chunk = HEADER;
  chunk.push_back(Tag::BeginFirmwareUpdate);
  const auto encoded = encode_3_to_16bit(payload);
  chunk.insert(chunk.end(), encoded.begin(), encoded.end());
  return chunk;
}

std::vector<uint8_t> make_bytes_chunk(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> chunk = HEADER;
  chunk.push_back(Tag::FirmwareBytes);
  const auto encoded = encode_8_to_7(data);
  chunk.insert(chunk.end(), encoded.begin(), encoded.end());
  return chunk;
}

std::vector<uint8_t> make_tag_chunk(uint8_t tag) {
  std::vector<uint8_t> chunk = HEADER;
  chunk.push_back(tag);
  return chunk;
}

struct Fixture {
  FakeWriter writer;
  musin::NullLogger logger;
  Update update{writer, logger};
  MockSender sender;

  Result handle(const std::vector<uint8_t> &chunk, absolute_time_t now = 0) {
    return update.handle_chunk(sysex::Chunk{chunk.data(), chunk.size()}, sender,
                               now);
  }
};

} // namespace

TEST_CASE("FirmwareUpdate full transfer happy path") {
  Fixture f;

  // 14 bytes => two 7-byte encode groups, no padding ambiguity
  const std::vector<uint8_t> firmware = {0x00, 0x80, 0xFF, 0x7F, 0x01,
                                         0x02, 0x03, 0x10, 0x20, 0x30,
                                         0x40, 0x50, 0x60, 0x70};

  REQUIRE(f.handle(make_begin_chunk(firmware.size())) == Result::OK);
  REQUIRE(f.sender.last() == Tag::Ack);
  REQUIRE(f.update.busy());
  REQUIRE(f.writer.begun_size == firmware.size());
  REQUIRE(f.writer.begun_sha == std::vector<uint8_t>(32, 0xAB));

  REQUIRE(f.handle(make_bytes_chunk(firmware)) == Result::OK);
  REQUIRE(f.sender.last() == Tag::Ack);
  REQUIRE(f.writer.written == firmware);
  REQUIRE(f.update.bytes_received() == firmware.size());

  REQUIRE(f.handle(make_tag_chunk(Tag::EndFirmwareUpdate)) ==
          Result::UpdateReady);
  REQUIRE(f.sender.last() == Tag::Ack);
  REQUIRE(f.writer.finalize_count == 1);
  REQUIRE_FALSE(f.update.busy());
}

TEST_CASE("FirmwareUpdate tolerates codec padding in the final group") {
  Fixture f;
  // 10 bytes is not a multiple of 7, so the encoded stream carries 4 padding
  // bytes in its final 8-byte group.
  const std::vector<uint8_t> firmware(10, 0x42);
  REQUIRE(f.handle(make_begin_chunk(firmware.size())) == Result::OK);
  REQUIRE(f.handle(make_bytes_chunk(firmware)) == Result::OK);
  REQUIRE(f.sender.last() == Tag::Ack);
  REQUIRE(f.writer.written == firmware);
  REQUIRE(f.handle(make_tag_chunk(Tag::EndFirmwareUpdate)) ==
          Result::UpdateReady);
}

TEST_CASE("FirmwareUpdate rejects Begin with zero size") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(0)) == Result::WriteError);
  REQUIRE(f.sender.last() == Tag::Nack);
  REQUIRE_FALSE(f.update.busy());
}

TEST_CASE("FirmwareUpdate rejects Begin when writer refuses") {
  Fixture f;
  f.writer.begin_ok = false;
  REQUIRE(f.handle(make_begin_chunk(1024)) == Result::WriteError);
  REQUIRE(f.sender.last() == Tag::Nack);
}

TEST_CASE("FirmwareUpdate rejects truncated Begin payload") {
  Fixture f;
  std::vector<uint8_t> chunk = HEADER;
  chunk.push_back(Tag::BeginFirmwareUpdate);
  const auto encoded = encode_3_to_16bit({0x00, 0x04}); // only 2 bytes
  chunk.insert(chunk.end(), encoded.begin(), encoded.end());
  REQUIRE(f.handle(chunk) == Result::InvalidContent);
  REQUIRE(f.sender.last() == Tag::Nack);
}

TEST_CASE("FirmwareUpdate rejects bytes without Begin") {
  Fixture f;
  REQUIRE(f.handle(make_bytes_chunk({1, 2, 3, 4, 5, 6, 7})) ==
          Result::InvalidContent);
  REQUIRE(f.sender.last() == Tag::Nack);
}

TEST_CASE("FirmwareUpdate rejects End without Begin") {
  Fixture f;
  REQUIRE(f.handle(make_tag_chunk(Tag::EndFirmwareUpdate)) ==
          Result::InvalidContent);
  REQUIRE(f.sender.last() == Tag::Nack);
}

TEST_CASE("FirmwareUpdate aborts on write failure") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(14)) == Result::OK);
  f.writer.write_ok = false;
  REQUIRE(f.handle(make_bytes_chunk(std::vector<uint8_t>(14, 0x55))) ==
          Result::WriteError);
  REQUIRE(f.sender.last() == Tag::Nack);
  REQUIRE(f.writer.abort_count == 1);
  REQUIRE_FALSE(f.update.busy());
}

TEST_CASE("FirmwareUpdate rejects overflow beyond announced size") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(7)) == Result::OK);
  REQUIRE(f.handle(make_bytes_chunk(std::vector<uint8_t>(14, 0x55))) ==
          Result::WriteError);
  REQUIRE(f.sender.last() == Tag::Nack);
  REQUIRE(f.writer.abort_count == 1);
}

TEST_CASE("FirmwareUpdate Nacks End on size mismatch") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(28)) == Result::OK);
  REQUIRE(f.handle(make_bytes_chunk(std::vector<uint8_t>(14, 0x55))) ==
          Result::OK);
  REQUIRE(f.handle(make_tag_chunk(Tag::EndFirmwareUpdate)) ==
          Result::WriteError);
  REQUIRE(f.sender.last() == Tag::Nack);
  REQUIRE(f.writer.abort_count == 1);
}

TEST_CASE("FirmwareUpdate Nacks End when verification fails") {
  Fixture f;
  f.writer.finalize_ok = false;
  REQUIRE(f.handle(make_begin_chunk(14)) == Result::OK);
  REQUIRE(f.handle(make_bytes_chunk(std::vector<uint8_t>(14, 0x55))) ==
          Result::OK);
  REQUIRE(f.handle(make_tag_chunk(Tag::EndFirmwareUpdate)) ==
          Result::WriteError);
  REQUIRE(f.sender.last() == Tag::Nack);
}

TEST_CASE("FirmwareUpdate host abort returns to idle") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(14)) == Result::OK);
  REQUIRE(f.handle(make_tag_chunk(Tag::AbortFirmwareUpdate)) ==
          Result::Aborted);
  REQUIRE(f.sender.last() == Tag::Ack);
  REQUIRE(f.writer.abort_count == 1);
  REQUIRE_FALSE(f.update.busy());
}

TEST_CASE("FirmwareUpdate restarts on Begin during active transfer") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(14)) == Result::OK);
  REQUIRE(f.handle(make_begin_chunk(28)) == Result::OK);
  REQUIRE(f.writer.abort_count == 1);
  REQUIRE(f.writer.begun_size == 28);
  REQUIRE(f.update.busy());
}

TEST_CASE("FirmwareUpdate times out after inactivity") {
  Fixture f;
  REQUIRE(f.handle(make_begin_chunk(14), 0) == Result::OK);
  REQUIRE_FALSE(f.update.check_timeout(Update::TIMEOUT_US));
  REQUIRE(f.update.check_timeout(Update::TIMEOUT_US + 1));
  REQUIRE(f.writer.abort_count == 1);
  REQUIRE_FALSE(f.update.busy());
}

TEST_CASE("FirmwareUpdate claims only its tags") {
  const auto begin = make_begin_chunk(14);
  REQUIRE(Update::claims(sysex::Chunk{begin.data(), begin.size()}));

  std::vector<uint8_t> file_bytes = HEADER;
  file_bytes.push_back(0x11); // existing FileBytes tag
  REQUIRE_FALSE(
      Update::claims(sysex::Chunk{file_bytes.data(), file_bytes.size()}));

  std::vector<uint8_t> wrong_mfr = {0x00, 0x21, 0x6B, 0x00,
                                    Tag::BeginFirmwareUpdate};
  REQUIRE_FALSE(
      Update::claims(sysex::Chunk{wrong_mfr.data(), wrong_mfr.size()}));
}
