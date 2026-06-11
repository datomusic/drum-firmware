#include "musin/flash/firmware_writer.h"

extern "C" {
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/error.h"
}

#include "etl/algorithm.h"

#include "musin/filesystem/partition_manager.h"

namespace musin::flash {

namespace {
constexpr uint32_t sector_base(uint32_t flash_offset) {
  return flash_offset & ~(FLASH_SECTOR_SIZE - 1u);
}
} // namespace

FirmwareWriter::FirmwareWriter(musin::Logger &logger) : logger_(logger) {
}

bool FirmwareWriter::begin(uint32_t total_size,
                           etl::span<const uint8_t> sha256) {
  if (receiving_) {
    abort();
  }
  image_ready_ = false;

  if (sha256.size() != SHA256_SIZE) {
    logger_.error("FirmwareWriter: Invalid SHA-256 length");
    return false;
  }

  if (!resolve_target_partition()) {
    return false;
  }

  // The UF2 stream is roughly twice the image size (512-byte blocks carrying
  // 256-byte payloads); the parser range-checks the actual payload addresses
  // against the partition, so only sanity-check the stream size here.
  if (total_size > 2 * target_size_ + 2 * Uf2Parser::BLOCK_SIZE) {
    logger_.error("FirmwareWriter: Announced size exceeds partition capacity");
    return false;
  }
  if (total_size % Uf2Parser::BLOCK_SIZE != 0) {
    logger_.error("FirmwareWriter: Size is not a multiple of UF2 block size");
    return false;
  }

  // No DMA: avoids claiming a channel next to the LED/audio DMA users, and
  // hashing speed is irrelevant against MIDI transfer speed.
  if (pico_sha256_try_start(&sha_state_, SHA256_BIG_ENDIAN, false) != PICO_OK) {
    logger_.error("FirmwareWriter: SHA-256 hardware unavailable");
    return false;
  }
  sha_active_ = true;

  parser_ = Uf2Parser{target_offset_, target_size_};
  etl::copy(sha256.begin(), sha256.end(), expected_sha_.begin());
  total_stream_size_ = total_size;
  stream_bytes_seen_ = 0;
  current_sector_base_ = NO_SECTOR;
  receiving_ = true;

  logger_.info("FirmwareWriter: Update started, target offset", target_offset_);
  return true;
}

bool FirmwareWriter::write(etl::span<const uint8_t> bytes) {
  if (!receiving_) {
    return false;
  }
  if (stream_bytes_seen_ + bytes.size() > total_stream_size_) {
    logger_.error("FirmwareWriter: Stream exceeds announced size");
    return false;
  }

  pico_sha256_update_blocking(&sha_state_, bytes.data(), bytes.size());
  stream_bytes_seen_ += bytes.size();

  const auto result =
      parser_.push(bytes, [this](const Uf2Parser::Block &block) {
        return stage_payload(block.flash_offset, block.payload);
      });

  if (result != Uf2Parser::Result::Ok &&
      result != Uf2Parser::Result::Complete) {
    logger_.error("FirmwareWriter: UF2 parse error",
                  static_cast<uint32_t>(result));
    return false;
  }
  return true;
}

bool FirmwareWriter::finalize() {
  if (!receiving_) {
    return false;
  }
  receiving_ = false;

  if (stream_bytes_seen_ != total_stream_size_ || !parser_.is_complete()) {
    logger_.error("FirmwareWriter: Incomplete UF2 stream");
    release_sha();
    return false;
  }

  if (!flush_sector()) {
    release_sha();
    return false;
  }

  sha256_result_t result;
  pico_sha256_finish(&sha_state_, &result);
  sha_active_ = false;

  if (!etl::equal(expected_sha_.begin(), expected_sha_.end(), result.bytes)) {
    logger_.error("FirmwareWriter: SHA-256 mismatch");
    return false;
  }

  logger_.info("FirmwareWriter: Image verified, bytes written",
               parser_.bytes_emitted());
  image_ready_ = true;
  return true;
}

void FirmwareWriter::abort() {
  receiving_ = false;
  image_ready_ = false;
  current_sector_base_ = NO_SECTOR;
  release_sha();
}

std::optional<uint32_t> FirmwareWriter::target_flash_offset() const {
  if (!image_ready_) {
    return std::nullopt;
  }
  return target_offset_;
}

bool FirmwareWriter::resolve_target_partition() {
  boot_info_t boot_info{};
  if (rom_get_boot_info(&boot_info) == 0) {
    logger_.error("FirmwareWriter: rom_get_boot_info failed");
    return false;
  }

  uint32_t target_id;
  if (boot_info.partition == FIRMWARE_A_PARTITION_ID) {
    target_id = FIRMWARE_B_PARTITION_ID;
  } else if (boot_info.partition == FIRMWARE_B_PARTITION_ID) {
    target_id = FIRMWARE_A_PARTITION_ID;
  } else {
    logger_.error("FirmwareWriter: Not booted from an A/B partition",
                  static_cast<int32_t>(boot_info.partition));
    return false;
  }

  musin::filesystem::PartitionManager partition_manager(logger_);
  const auto target = partition_manager.find_partition(target_id);
  const auto booted = partition_manager.find_partition(boot_info.partition);
  if (!target.has_value() || !booted.has_value()) {
    logger_.error("FirmwareWriter: Failed to resolve firmware partitions");
    return false;
  }

  // Never allow the target range to touch the running image.
  if (target->offset == booted->offset) {
    logger_.error("FirmwareWriter: Target partition equals booted partition");
    return false;
  }

  target_offset_ = target->offset;
  target_size_ = target->size;
  return true;
}

bool FirmwareWriter::stage_payload(uint32_t flash_offset,
                                   etl::span<const uint8_t> payload) {
  const uint32_t base = sector_base(flash_offset);
  if (base != current_sector_base_) {
    if (!flush_sector()) {
      return false;
    }
    current_sector_base_ = base;
    sector_buffer_.fill(0xFF);
  }

  etl::copy(payload.begin(), payload.end(),
            sector_buffer_.begin() + (flash_offset - base));
  return true;
}

bool FirmwareWriter::flush_sector() {
  if (current_sector_base_ == NO_SECTOR) {
    return true;
  }
  if (current_sector_base_ < target_offset_ ||
      current_sector_base_ + FLASH_SECTOR_SIZE >
          target_offset_ + target_size_) {
    logger_.error("FirmwareWriter: Sector outside target partition");
    return false;
  }

  watchdog_update();
  const uint32_t saved_irqs = save_and_disable_interrupts();
  flash_range_erase(current_sector_base_, FLASH_SECTOR_SIZE);
  flash_range_program(current_sector_base_, sector_buffer_.data(),
                      FLASH_SECTOR_SIZE);
  restore_interrupts(saved_irqs);
  watchdog_update();

  current_sector_base_ = NO_SECTOR;
  return true;
}

void FirmwareWriter::release_sha() {
  if (sha_active_) {
    pico_sha256_cleanup(&sha_state_);
    sha_active_ = false;
  }
}

} // namespace musin::flash
