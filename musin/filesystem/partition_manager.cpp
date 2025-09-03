// clang-format off
extern "C" {
#include "pico.h"
#include "blockdevice/flash.h"
#include "boot/bootrom_constants.h"
#include "boot/picobin.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include <stdbool.h>
#include <stdio.h>
}
// clang-format on

#include "partition_manager.h"

namespace {

#define PART_LOC_FIRST(x)                                                      \
  (((x) & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>                     \
   PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB)
#define PART_LOC_LAST(x)                                                       \
  (((x) & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>                      \
   PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB)

} // namespace

namespace musin::filesystem {

uint8_t PartitionManager::pt_work_area_[PARTITION_WORK_AREA_SIZE];

PartitionManager::PartitionManager(musin::Logger &logger) : logger_(logger) {
}

std::optional<PartitionInfo>
PartitionManager::find_partition(uint32_t partition_id) {
  logger_.info("Finding partition with ID: ", partition_id);

  uint32_t partition_table_info[PARTITION_TABLE_INFO_BUFFER_SIZE];
  const uint32_t flags = PT_INFO_PT_INFO |
                         PT_INFO_PARTITION_LOCATION_AND_FLAGS |
                         PT_INFO_PARTITION_ID;

  int result = rom_get_partition_table_info(
      partition_table_info, sizeof(partition_table_info), flags);

  if (result <= 0) {
    logger_.error("Could not get partition table info, error: ",
                  static_cast<std::int32_t>(result));
    return std::nullopt;
  }

  size_t pos = 0;
  uint32_t fields = partition_table_info[pos++];
  if (fields != flags) {
    logger_.error("Partition table fields mismatch.");
    logger_.error("  - Expected: ", flags);
    logger_.error("  - Actual:   ", fields);
    return std::nullopt;
  }

  bool has_partition_table =
      partition_table_info[pos] & HAS_PARTITION_TABLE_FLAG;
  if (!has_partition_table) {
    logger_.error("No partition table found on device.");
    return std::nullopt;
  }
  uint32_t partition_count = partition_table_info[pos] & PARTITION_COUNT_MASK;
  pos++;

  // Skip unpartitioned space info
  pos++; // unpartitioned_space location
  pos++; // unpartitioned_space flags

  if (partition_count == 0) {
    logger_.warn("No partitions found in partition table");
    return std::nullopt;
  }

  for (uint32_t i = 0; i < partition_count; i++) {
    uint32_t location = partition_table_info[pos++];
    uint32_t flags_and_permissions = partition_table_info[pos++];
    bool has_id = flags_and_permissions & PICOBIN_PARTITION_FLAGS_HAS_ID_BITS;

    uint64_t current_partition_id = 0;
    if (has_id) {
      uint32_t id_low = partition_table_info[pos++];
      uint32_t id_high = partition_table_info[pos++];
      current_partition_id = ((uint64_t)id_high << 32) | id_low;
    }

    if (has_id && current_partition_id == static_cast<uint64_t>(partition_id)) {
      uint32_t first_sector = PART_LOC_FIRST(location);
      uint32_t last_sector = PART_LOC_LAST(location);
      uint32_t offset = first_sector * FLASH_SECTOR_SIZE;
      uint32_t size = (last_sector + 1 - first_sector) * FLASH_SECTOR_SIZE;

      logger_.info("Found partition ID ", partition_id);
      logger_.info("  - Offset: ", offset);
      logger_.info("  - Size:   ", size);

      return PartitionInfo{offset, size, flags_and_permissions};
    }
  }

  logger_.error("Partition ID not found: ", partition_id);
  return std::nullopt;
}

std::optional<PartitionInfo>
PartitionManager::find_partition_by_family(uint32_t family_bit) {
  logger_.info("Finding partition with family bit: ", family_bit);

  uint32_t partition_table_info[PARTITION_TABLE_INFO_BUFFER_SIZE];
  const uint32_t flags = PT_INFO_PT_INFO | PT_INFO_PARTITION_LOCATION_AND_FLAGS;

  int result = rom_get_partition_table_info(
      partition_table_info, sizeof(partition_table_info), flags);

  if (result <= 0) {
    logger_.error("Could not get partition table info, error: ",
                  static_cast<std::int32_t>(result));
    return std::nullopt;
  }

  size_t pos = 0;
  uint32_t fields = partition_table_info[pos++];
  if (fields != flags) {
    logger_.error("Partition table fields mismatch.");
    logger_.error("  - Expected: ", flags);
    logger_.error("  - Actual:   ", fields);
    return std::nullopt;
  }

  bool has_partition_table =
      partition_table_info[pos] & HAS_PARTITION_TABLE_FLAG;
  if (!has_partition_table) {
    logger_.error("No partition table found on device.");
    return std::nullopt;
  }
  uint32_t partition_count = partition_table_info[pos] & PARTITION_COUNT_MASK;
  pos++; // Skip partition_count and has_partition_table_flag
  pos++; // Skip unpartitioned_space location
  pos++; // Skip unpartitioned_space flags

  if (partition_count == 0) {
    logger_.warn("No partitions found in partition table");
    return std::nullopt;
  }

  for (uint32_t i = 0; i < partition_count; i++) {
    uint32_t location = partition_table_info[pos++];
    uint32_t flags_and_permissions = partition_table_info[pos++];

    if (flags_and_permissions & family_bit) {
      uint32_t first_sector = PART_LOC_FIRST(location);
      uint32_t last_sector = PART_LOC_LAST(location);
      uint32_t offset = first_sector * FLASH_SECTOR_SIZE;
      uint32_t size = (last_sector + 1 - first_sector) * FLASH_SECTOR_SIZE;

      logger_.info("Found matching partition at index ", i);
      logger_.info("  - Offset: ", offset);
      logger_.info("  - Size:   ", size);

      return PartitionInfo{offset, size, flags_and_permissions};
    }
  }

  logger_.error("Partition with specified family bit not found: ", family_bit);
  return std::nullopt;
}

blockdevice_t *PartitionManager::create_partition_blockdevice(
    const PartitionInfo &partition_info) {
  logger_.info("Creating block device for partition");

  // Align partition boundaries to FLASH_SECTOR_SIZE (4096 bytes)
  // Round start address UP to stay within partition bounds
  uint32_t aligned_start =
      (partition_info.offset + FLASH_SECTOR_ALIGNMENT_MASK) &
      ~FLASH_SECTOR_ALIGNMENT_MASK;

  // Calculate end address and round DOWN to stay within partition bounds
  uint32_t partition_end = partition_info.offset + partition_info.size;
  uint32_t aligned_end = partition_end & ~FLASH_SECTOR_ALIGNMENT_MASK;

  // Ensure we have at least one sector after alignment
  if (aligned_start >= aligned_end) {
    logger_.error("Partition too small after alignment, needs bytes: ",
                  static_cast<std::int32_t>(FLASH_SECTOR_SIZE));
    return nullptr;
  }

  uint32_t aligned_size = aligned_end - aligned_start;

  logger_.info("Original partition offset: ", partition_info.offset);
  logger_.info("Original partition size: ", partition_info.size);
  logger_.info("Aligned partition offset: ", aligned_start);
  logger_.info("Aligned partition size: ", aligned_size);

  blockdevice_t *flash = blockdevice_flash_create(aligned_start, aligned_size);

  if (!flash) {
    logger_.error("Failed to create flash block device");
  }

  return flash;
}

} // namespace musin::filesystem
