#ifndef MUSIN_FLASH_FIRMWARE_WRITER_H_ZR7TQ2BD
#define MUSIN_FLASH_FIRMWARE_WRITER_H_ZR7TQ2BD

#include <cstdint>
#include <optional>

#include "etl/array.h"
#include "etl/span.h"

extern "C" {
#include "hardware/flash.h"
#include "pico/sha256.h"
}

#include "musin/flash/uf2_parser.h"
#include "musin/hal/logger.h"

namespace musin::flash {

// Streams a UF2 image into the inactive A/B firmware partition.
//
// Receives the raw UF2 byte stream, feeds it through Uf2Parser (which rebases
// payload addresses to the target partition), stages payloads into a
// sector-sized buffer and erases/programs flash one sector at a time. A
// running hardware SHA-256 over the raw stream is checked in finalize().
class FirmwareWriter {
public:
  static constexpr size_t SHA256_SIZE = 32;
  static constexpr uint32_t FIRMWARE_A_PARTITION_ID = 0;
  static constexpr uint32_t FIRMWARE_B_PARTITION_ID = 1;

  explicit FirmwareWriter(musin::Logger &logger);

  // Resolves the inactive partition and prepares for a stream of total_size
  // raw UF2 bytes whose SHA-256 must equal sha256 (32 bytes).
  bool begin(uint32_t total_size, etl::span<const uint8_t> sha256);

  // Consumes the next raw UF2 stream bytes; programs flash as sectors fill.
  bool write(etl::span<const uint8_t> bytes);

  // Flushes the final sector and verifies stream completeness and SHA-256.
  // After a successful finalize, target_flash_offset() identifies the image
  // to pass to rom_reboot for a flash-update (try-before-you-buy) boot.
  bool finalize();

  void abort();

  std::optional<uint32_t> target_flash_offset() const;

private:
  bool resolve_target_partition();
  bool stage_payload(uint32_t flash_offset, etl::span<const uint8_t> payload);
  bool flush_sector();
  void release_sha();

  musin::Logger &logger_;
  Uf2Parser parser_{0, 0};
  pico_sha256_state_t sha_state_{};
  bool sha_active_ = false;
  bool receiving_ = false;
  bool image_ready_ = false;

  uint32_t target_offset_ = 0;
  uint32_t target_size_ = 0;
  uint32_t total_stream_size_ = 0;
  uint32_t stream_bytes_seen_ = 0;
  etl::array<uint8_t, SHA256_SIZE> expected_sha_{};

  static constexpr uint32_t NO_SECTOR = 0xFFFFFFFF;
  uint32_t current_sector_base_ = NO_SECTOR;
  etl::array<uint8_t, FLASH_SECTOR_SIZE> sector_buffer_{};
};

} // namespace musin::flash

#endif /* end of include guard: MUSIN_FLASH_FIRMWARE_WRITER_H_ZR7TQ2BD */
