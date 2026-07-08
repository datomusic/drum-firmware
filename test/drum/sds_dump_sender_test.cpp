#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"
#include "etl/vector.h"

#include "musin/hal/null_logger.h"

#include "drum/sysex/sds_dump_sender.h"

extern absolute_time_t mock_current_time;

namespace {

// File mock backed by an in-memory sample. open_read succeeds only for the
// configured content; an empty content simulates a missing file.
struct TestFileOps {
  struct ReadHandle {
    TestFileOps &parent;
    size_t position = 0;

    explicit ReadHandle(TestFileOps &parent) : parent(parent) {
      parent.file_is_open = true;
    }

    ReadHandle(const ReadHandle &) = delete;
    ReadHandle &operator=(const ReadHandle &) = delete;

    ReadHandle(ReadHandle &&other) noexcept
        : parent(other.parent), position(other.position) {
      other.owns_file = false;
    }

    size_t size() const {
      return parent.content.size();
    }

    size_t read(const etl::span<uint8_t> &out) {
      size_t count = 0;
      while (count < out.size() && position < parent.content.size()) {
        out[count++] = parent.content[position++];
      }
      return count;
    }

    ~ReadHandle() {
      if (owns_file) {
        parent.file_is_open = false;
      }
    }

  private:
    bool owns_file = true;
  };

  etl::optional<ReadHandle> open_read(const etl::string_view &path) {
    last_opened_path.assign(path.begin(), path.end());
    open_count++;
    if (content.empty()) {
      return etl::nullopt;
    }
    return etl::optional<ReadHandle>(ReadHandle(*this));
  }

  etl::vector<char, 32> last_opened_path;
  etl::vector<uint8_t, 512> content;
  bool file_is_open = false;
  unsigned open_count = 0;
};

using Sender = sds::DumpSender<TestFileOps>;

// Records every outgoing SDS message payload. Holds a reference to shared
// storage so pass-by-value copies inside the sender still record here.
using MessageLog = etl::vector<etl::vector<uint8_t, 128>, 32>;
struct MockSender {
  MessageLog &messages;
  void operator()(const etl::span<const uint8_t> &payload) {
    messages.emplace_back();
    messages.back().assign(payload.begin(), payload.end());
  }
};

constexpr uint64_t ONE_SECOND_US = 1000000;

etl::array<uint8_t, 3> make_dump_request(uint16_t sample_number) {
  return {sds::DUMP_REQUEST, static_cast<uint8_t>(sample_number & 0x7F),
          static_cast<uint8_t>((sample_number >> 7) & 0x7F)};
}

// Decodes one 16-bit sample from the SDS left-justified 3-byte format.
int16_t unpack_sample(const uint8_t *bytes) {
  const uint16_t unsigned_sample =
      ((static_cast<uint16_t>(bytes[0]) & 0x7F) << 9) |
      ((static_cast<uint16_t>(bytes[1]) & 0x7F) << 2) |
      ((static_cast<uint16_t>(bytes[2]) & 0x7F) >> 5);
  return static_cast<int16_t>(unsigned_sample - 0x8000);
}

void fill_with_samples(etl::vector<uint8_t, 512> &content,
                       std::initializer_list<int16_t> samples) {
  for (const int16_t s : samples) {
    content.push_back(static_cast<uint8_t>(s & 0xFF));
    content.push_back(static_cast<uint8_t>((s >> 8) & 0xFF));
  }
}

TEST_CASE("Dump request sends a header describing the stored sample") {
  TestFileOps file_ops;
  fill_with_samples(file_ops.content, {0, 100, -100, 32767});
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(30);
  const auto result = dump_sender.handle_dump_request(
      etl::span<const uint8_t>{request}, out, ONE_SECOND_US);

  REQUIRE(result == sds::Result::OK);
  REQUIRE(dump_sender.is_busy());
  REQUIRE(etl::string_view(file_ops.last_opened_path.data(),
                           file_ops.last_opened_path.size()) == "/30.pcm");

  REQUIRE(log.size() == 1);
  const auto &header = log[0];
  REQUIRE(header.size() == 17);
  REQUIRE(header[0] == sds::DUMP_HEADER);
  REQUIRE(header[1] == 30); // sample number low
  REQUIRE(header[2] == 0);  // sample number high
  REQUIRE(header[3] == 16); // bit depth

  const uint32_t period_ns = header[4] | (header[5] << 7) | (header[6] << 14);
  REQUIRE(1000000000U / period_ns == 44101); // integer-truncated 44.1 kHz

  const uint32_t length_words =
      header[7] | (header[8] << 7) | (header[9] << 14);
  REQUIRE(length_words == 4);
  REQUIRE(header[16] == 0x7F); // no loop
}

TEST_CASE("Dump request for a missing sample replies CANCEL and stays idle") {
  TestFileOps file_ops; // empty content = missing file
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(12);
  const auto result = dump_sender.handle_dump_request(
      etl::span<const uint8_t>{request}, out, ONE_SECOND_US);

  REQUIRE(result == sds::Result::FileError);
  REQUIRE_FALSE(dump_sender.is_busy());
  REQUIRE(log.size() == 1);
  REQUIRE(log[0][0] == sds::CANCEL);
}

TEST_CASE("ACK-driven dump round-trips sample data with valid checksums") {
  TestFileOps file_ops;
  // 42 samples: one full packet (40) plus a zero-padded second packet.
  etl::vector<int16_t, 64> samples;
  for (int i = 0; i < 42; ++i) {
    samples.push_back(static_cast<int16_t>(i * 700 - 14000));
  }
  for (const int16_t s : samples) {
    file_ops.content.push_back(static_cast<uint8_t>(s & 0xFF));
    file_ops.content.push_back(static_cast<uint8_t>((s >> 8) & 0xFF));
  }

  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(5);
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out,
                                  ONE_SECOND_US);

  // Host ACKs the header, then each packet.
  REQUIRE(dump_sender.handle_response(sds::ACK, out, ONE_SECOND_US) ==
          sds::Result::OK);
  REQUIRE(dump_sender.handle_response(sds::ACK, out, ONE_SECOND_US) ==
          sds::Result::OK);
  REQUIRE(dump_sender.handle_response(sds::ACK, out, ONE_SECOND_US) ==
          sds::Result::SampleComplete);
  REQUIRE_FALSE(dump_sender.is_busy());
  REQUIRE_FALSE(file_ops.file_is_open);

  // Header + 2 data packets.
  REQUIRE(log.size() == 3);
  size_t sample_index = 0;
  for (size_t p = 1; p < log.size(); ++p) {
    const auto &packet = log[p];
    REQUIRE(packet.size() == 123);
    REQUIRE(packet[0] == sds::DATA_PACKET);
    REQUIRE(packet[1] == p - 1); // packet number

    const auto data = etl::span<const uint8_t>{&packet[2], 120};
    REQUIRE(packet[122] == sds::calculate_data_checksum(packet[1], data));

    for (size_t i = 0; i < 40; ++i) {
      const int16_t decoded = unpack_sample(&packet[2 + i * 3]);
      const int16_t expected =
          sample_index < samples.size() ? samples[sample_index] : 0;
      REQUIRE(decoded == expected);
      ++sample_index;
    }
  }
}

TEST_CASE("NAK resends the previous message unchanged") {
  TestFileOps file_ops;
  fill_with_samples(file_ops.content, {1000, -1000});
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(7);
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out,
                                  ONE_SECOND_US);

  SECTION("NAK after the header resends the header") {
    dump_sender.handle_response(sds::NAK, out, ONE_SECOND_US);
    REQUIRE(log.size() == 2);
    REQUIRE(log[1] == log[0]);
  }

  SECTION("NAK after a data packet resends that packet") {
    dump_sender.handle_response(sds::ACK, out, ONE_SECOND_US); // packet 0
    dump_sender.handle_response(sds::NAK, out, ONE_SECOND_US);
    REQUIRE(log.size() == 3);
    REQUIRE(log[2] == log[1]);
    // The retransmitted packet still completes the transfer on ACK.
    REQUIRE(dump_sender.handle_response(sds::ACK, out, ONE_SECOND_US) ==
            sds::Result::SampleComplete);
  }
}

TEST_CASE("CANCEL from the host aborts the dump and closes the file") {
  TestFileOps file_ops;
  fill_with_samples(file_ops.content, {1, 2, 3, 4});
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(3);
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out,
                                  ONE_SECOND_US);
  REQUIRE(file_ops.file_is_open);

  REQUIRE(dump_sender.handle_response(sds::CANCEL, out, ONE_SECOND_US) ==
          sds::Result::Cancelled);
  REQUIRE_FALSE(dump_sender.is_busy());
  REQUIRE_FALSE(file_ops.file_is_open);
}

TEST_CASE("A silent host gets the dump open-loop") {
  TestFileOps file_ops;
  fill_with_samples(file_ops.content, {500, -500, 250, -250});
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(9);
  absolute_time_t now = ONE_SECOND_US;
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out, now);

  // No response within the 2 s header window: first packet goes out anyway.
  now += Sender::HEADER_RESPONSE_TIMEOUT_US + 1;
  REQUIRE(dump_sender.update(out, now) == sds::Result::OK);
  REQUIRE(log.size() == 2);
  REQUIRE(log[1][0] == sds::DATA_PACKET);

  // The single packet covers all 4 samples; the next interval completes.
  now += Sender::OPEN_LOOP_INTERVAL_US + 1;
  REQUIRE(dump_sender.update(out, now) == sds::Result::SampleComplete);
  REQUIRE_FALSE(dump_sender.is_busy());
  REQUIRE_FALSE(file_ops.file_is_open);
}

TEST_CASE("A handshaking host with a lost packet gets retransmissions") {
  TestFileOps file_ops;
  // Two packets' worth of data so the transfer is not complete after one.
  etl::vector<int16_t, 64> samples;
  for (int i = 0; i < 42; ++i) {
    samples.push_back(static_cast<int16_t>(i));
  }
  for (const int16_t s : samples) {
    file_ops.content.push_back(static_cast<uint8_t>(s & 0xFF));
    file_ops.content.push_back(static_cast<uint8_t>((s >> 8) & 0xFF));
  }
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(4);
  absolute_time_t now = ONE_SECOND_US;
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out, now);
  dump_sender.handle_response(sds::ACK, out, now); // header ACK, packet 0 sent
  REQUIRE(log.size() == 2);

  // Packet 0 (or our view of its ACK) is lost: the host stays silent. The
  // device must retransmit the same packet, not advance past it.
  now += Sender::RETRANSMIT_INTERVAL_US + 1;
  REQUIRE(dump_sender.update(out, now) == sds::Result::OK);
  REQUIRE(log.size() == 3);
  REQUIRE(log[2] == log[1]); // identical retransmission

  // Still silent: it keeps retransmitting at the same cadence.
  now += Sender::RETRANSMIT_INTERVAL_US + 1;
  dump_sender.update(out, now);
  REQUIRE(log.size() == 4);
  REQUIRE(log[3] == log[1]);

  // The late ACK finally arrives and the transfer resumes with packet 1.
  dump_sender.handle_response(sds::ACK, out, now);
  REQUIRE(log.size() == 5);
  REQUIRE(log[4][1] == 1); // next packet number, not another retransmit

  // A host that never comes back eventually trips the stall timeout.
  now += Sender::STALL_TIMEOUT_US + 1;
  REQUIRE(dump_sender.update(out, now) == sds::Result::Cancelled);
  REQUIRE_FALSE(dump_sender.is_busy());
}

TEST_CASE("A host that sends WAIT and then vanishes trips the stall timeout") {
  TestFileOps file_ops;
  fill_with_samples(file_ops.content, {500, -500});
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(2);
  absolute_time_t now = ONE_SECOND_US;
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out, now);
  dump_sender.handle_response(sds::WAIT, out, now);

  // WAIT suppresses open-loop fallback...
  now += 10 * ONE_SECOND_US;
  REQUIRE(dump_sender.update(out, now) == sds::Result::OK);
  REQUIRE(dump_sender.is_busy());
  REQUIRE(log.size() == 1); // only the header went out

  // ...but not the stall timeout.
  now += Sender::STALL_TIMEOUT_US;
  REQUIRE(dump_sender.update(out, now) == sds::Result::Cancelled);
  REQUIRE_FALSE(dump_sender.is_busy());
  REQUIRE_FALSE(file_ops.file_is_open);
}

TEST_CASE("A second dump request during an active dump is refused") {
  TestFileOps file_ops;
  fill_with_samples(file_ops.content, {1, 2});
  musin::NullLogger logger;
  Sender dump_sender(file_ops, logger);
  MessageLog log;
  MockSender out{log};

  const auto request = make_dump_request(1);
  dump_sender.handle_dump_request(etl::span<const uint8_t>{request}, out,
                                  ONE_SECOND_US);
  REQUIRE(log.size() == 1);

  const auto result = dump_sender.handle_dump_request(
      etl::span<const uint8_t>{request}, out, ONE_SECOND_US);
  REQUIRE(result == sds::Result::StateError);
  REQUIRE(log.back()[0] == sds::CANCEL);
  REQUIRE(dump_sender.is_busy()); // original dump unaffected
}

} // namespace
