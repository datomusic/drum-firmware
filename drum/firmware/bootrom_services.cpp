#include "drum/firmware/bootrom_services.h"

#include <cstddef>

#include "etl/optional.h"

extern "C" {
#include "pico.h"
#include "boot/bootrom_constants.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/bootrom.h"
}

namespace {

constexpr std::uint32_t kPartitionTableWorkAreaBytes = 4096U;
constexpr std::uint32_t kFirmwarePartitionIdA = 0U;
constexpr std::uint32_t kFirmwarePartitionIdB = 1U;

etl::optional<std::uint32_t> to_storage_addr(std::uint32_t runtime_address) {
  const intptr_t result = rom_flash_runtime_to_storage_addr(
      static_cast<uintptr_t>(runtime_address));
  if (result < 0) {
    return etl::nullopt;
  }
  return static_cast<std::uint32_t>(result);
}

cflash_flags_t make_flash_flags(std::uint32_t op, std::uint32_t aspace) {
  cflash_flags_t flags{};
  flags.flags = ((aspace << CFLASH_ASPACE_LSB) & CFLASH_ASPACE_BITS) |
                ((CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) &
                 CFLASH_SECLEVEL_BITS) |
                ((op << CFLASH_OP_LSB) & CFLASH_OP_BITS);
  return flags;
}

} // namespace

namespace drum::firmware {

namespace {
uint8_t partition_table_work_area[kPartitionTableWorkAreaBytes];
} // namespace

static_assert(BootRomPartitionFlashWriter::BUFFER_SIZE == FLASH_PAGE_SIZE,
              "Flash writer buffer must match flash page size");

BootRomFirmwarePartitionManager::BootRomFirmwarePartitionManager(
    musin::Logger &logger,
    musin::filesystem::PartitionManager &partition_manager)
    : logger_(logger), partition_manager_(partition_manager) {
}

etl::optional<PartitionRegion> BootRomFirmwarePartitionManager::begin_staging(
    const FirmwareImageMetadata &metadata) {
  if (staging_active_) {
    logger_.warn("FirmwarePartitionManager: staging already active");
    return etl::nullopt;
  }

  if (!refresh_partition_layout()) {
    logger_.error("FirmwarePartitionManager: failed to refresh layout");
    return etl::nullopt;
  }

  const bool active_is_a = active_slot_id_ == kFirmwarePartitionIdA;
  const SlotInfo &target = active_is_a ? slot_b_ : slot_a_;

  if (!target.valid) {
    logger_.error("FirmwarePartitionManager: inactive slot unavailable");
    return etl::nullopt;
  }

  if (metadata.declared_size > target.region.length) {
    logger_.error("FirmwarePartitionManager: image too large for slot");
    return etl::nullopt;
  }

  staging_active_ = true;
  staging_region_ = target.region;
  staging_metadata_ = metadata;
  return target.region;
}

void BootRomFirmwarePartitionManager::abort_staging() {
  staging_active_ = false;
  staging_region_ = PartitionRegion{0U, 0U};
  staging_metadata_ = FirmwareImageMetadata{};
}

PartitionError BootRomFirmwarePartitionManager::commit_staging(
    const FirmwareImageMetadata &metadata) {
  if (!staging_active_) {
    logger_.error("FirmwarePartitionManager: commit without staging");
    return PartitionError::UnexpectedState;
  }

  if (metadata.declared_size != staging_metadata_.declared_size) {
    logger_.warn(
        "FirmwarePartitionManager: metadata size changed during staging");
  }

  staging_active_ = false;
  staging_region_ = PartitionRegion{0U, 0U};
  staging_metadata_ = FirmwareImageMetadata{};
  return PartitionError::None;
}

bool BootRomFirmwarePartitionManager::refresh_partition_layout() {
  if (!load_partition_table()) {
    return false;
  }

  slot_a_ = SlotInfo{};
  slot_b_ = SlotInfo{};

  const auto slot_a_info =
      partition_manager_.find_partition(kFirmwarePartitionIdA);
  if (!slot_a_info.has_value()) {
    logger_.error("FirmwarePartitionManager: firmware slot A missing");
    return false;
  }

  const auto slot_b_info =
      partition_manager_.find_partition(kFirmwarePartitionIdB);
  if (!slot_b_info.has_value()) {
    logger_.error("FirmwarePartitionManager: firmware slot B missing");
    return false;
  }

  slot_a_.region =
      PartitionRegion{slot_a_info->offset, slot_a_info->size};
  slot_a_.valid = true;
  slot_b_.region =
      PartitionRegion{slot_b_info->offset, slot_b_info->size};
  slot_b_.valid = true;

  return determine_active_slot();
}

bool BootRomFirmwarePartitionManager::load_partition_table() {
  const int rc = rom_load_partition_table(
      partition_table_work_area, sizeof(partition_table_work_area), false);
  if (rc < 0 && rc != BOOTROM_ERROR_INVALID_STATE) {
    logger_.error("FirmwarePartitionManager: load_partition_table failed:",
                  static_cast<std::int32_t>(rc));
    return false;
  }
  return true;
}

bool BootRomFirmwarePartitionManager::determine_active_slot() {
  if (!slot_a_.valid || !slot_b_.valid) {
    logger_.error("FirmwarePartitionManager: firmware slots missing");
    return false;
  }

  const auto active_storage_base =
      to_storage_addr(static_cast<std::uint32_t>(XIP_BASE));
  if (!active_storage_base.has_value()) {
    logger_.error("FirmwarePartitionManager: failed to translate runtime addr");
    return false;
  }

  if (*active_storage_base == slot_a_.region.offset) {
    active_slot_id_ = kFirmwarePartitionIdA;
    return true;
  }

  if (*active_storage_base == slot_b_.region.offset) {
    active_slot_id_ = kFirmwarePartitionIdB;
    return true;
  }

  logger_.error("FirmwarePartitionManager: active slot unknown");
  return false;
}

BootRomPartitionFlashWriter::BootRomPartitionFlashWriter(musin::Logger &logger)
    : logger_(logger) {
}

std::size_t BootRomPartitionFlashWriter::page_size_bytes() const {
  return BUFFER_SIZE;
}

std::size_t BootRomPartitionFlashWriter::max_chunk_size_bytes() const {
  return BUFFER_SIZE;
}

bool BootRomPartitionFlashWriter::begin(const PartitionRegion &region,
                                        const FirmwareImageMetadata &metadata) {
  if (busy_) {
    logger_.error("PartitionFlashWriter: begin called while busy");
    return false;
  }

  if (metadata.declared_size > region.length) {
    logger_.error("PartitionFlashWriter: metadata larger than region");
    return false;
  }

  reset_state();
  busy_ = true;
  region_ = region;
  metadata_ = metadata;
  return true;
}

bool BootRomPartitionFlashWriter::write_chunk(
    const etl::span<const std::uint8_t> &chunk) {
  if (!busy_) {
    logger_.error("PartitionFlashWriter: write without begin");
    return false;
  }

  for (std::uint8_t byte : chunk) {
    if (bytes_written_ + static_cast<std::uint32_t>(buffer_count_) >=
        metadata_.declared_size) {
      logger_.error("PartitionFlashWriter: received data beyond declared size");
      return false;
    }

    if (buffer_count_ == 0U) {
      buffer_base_offset_ = bytes_written_;
    }

    if (buffer_count_ >= buffer_.size()) {
      if (!flush_buffer()) {
        return false;
      }
      buffer_base_offset_ = bytes_written_;
    }

    buffer_[buffer_count_++] = byte;

    if (buffer_count_ == buffer_.size()) {
      if (!flush_buffer()) {
        return false;
      }
    }
  }

  return true;
}

bool BootRomPartitionFlashWriter::finalize() {
  if (!busy_) {
    logger_.error("PartitionFlashWriter: finalize without active session");
    return false;
  }

  if (buffer_count_ > 0U) {
    for (std::size_t i = buffer_count_; i < buffer_.size(); ++i) {
      buffer_[i] = 0xFFU;
    }

    if (!flush_buffer()) {
      cancel();
      return false;
    }
  }

  if (bytes_written_ != metadata_.declared_size) {
    logger_.error("PartitionFlashWriter: bytes written mismatch");
    cancel();
    return false;
  }

  busy_ = false;
  return true;
}

void BootRomPartitionFlashWriter::cancel() {
  reset_state();
}

std::uint32_t BootRomPartitionFlashWriter::bytes_written() const {
  return bytes_written_;
}

bool BootRomPartitionFlashWriter::ensure_erased(std::uint32_t relative_offset,
                                                std::uint32_t length) {
  const std::uint32_t required_end =
      align_up(relative_offset + length, FLASH_SECTOR_SIZE);

  while (erased_bytes_ < required_end) {
    if (erased_bytes_ >= region_.length) {
      logger_.error("PartitionFlashWriter: erase beyond region size");
      return false;
    }
    const std::uint32_t sector_addr = region_.offset + erased_bytes_;
    const cflash_flags_t flags =
        make_flash_flags(CFLASH_OP_VALUE_ERASE, CFLASH_ASPACE_VALUE_STORAGE);
    const int rc = rom_flash_op(flags, sector_addr, FLASH_SECTOR_SIZE, nullptr);
    if (rc != BOOTROM_OK) {
      logger_.error("PartitionFlashWriter: erase failed:",
                    static_cast<std::int32_t>(rc));
      return false;
    }
    erased_bytes_ += FLASH_SECTOR_SIZE;
  }

  return true;
}

bool BootRomPartitionFlashWriter::flush_buffer() {
  if (buffer_count_ == 0U) {
    return true;
  }

  if (!ensure_erased(buffer_base_offset_, buffer_.size())) {
    return false;
  }

  const std::uint32_t absolute_offset = region_.offset + buffer_base_offset_;
  if ((absolute_offset + buffer_.size()) > (region_.offset + region_.length)) {
    logger_.error("PartitionFlashWriter: flush exceeds region bounds");
    return false;
  }

  const cflash_flags_t flags =
      make_flash_flags(CFLASH_OP_VALUE_PROGRAM, CFLASH_ASPACE_VALUE_STORAGE);
  const int rc =
      rom_flash_op(flags, absolute_offset, buffer_.size(), buffer_.data());
  if (rc != BOOTROM_OK) {
    logger_.error("PartitionFlashWriter: program failed:",
                  static_cast<std::int32_t>(rc));
    return false;
  }

  bytes_written_ += static_cast<std::uint32_t>(buffer_count_);
  buffer_count_ = 0U;
  return true;
}

void BootRomPartitionFlashWriter::reset_state() {
  busy_ = false;
  region_ = PartitionRegion{0U, 0U};
  metadata_ = FirmwareImageMetadata{};
  bytes_written_ = 0U;
  erased_bytes_ = 0U;
  buffer_base_offset_ = 0U;
  buffer_count_ = 0U;
  for (auto &byte : buffer_) {
    byte = 0xFFU;
  }
}

} // namespace drum::firmware
