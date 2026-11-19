// clang-format off
extern "C" {
#include "pico.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
}
// clang-format on

#include "musin/filesystem/storage_region.h"
#include "partition_manager.h"

namespace musin::filesystem {

// The Pico port answers "where does the data region live" via the RP2350
// bootrom partition table: the first partition accepting the data family.
// The region is aligned inward to flash sector boundaries so it is directly
// usable as a block device.
std::optional<StorageRegion> get_storage_region(musin::Logger &logger) {
  PartitionManager partition_manager(logger);
  auto partition_info = partition_manager.find_partition_by_family(
      PICOBIN_PARTITION_FLAGS_ACCEPTS_DEFAULT_FAMILY_DATA_BITS);
  if (!partition_info) {
    logger.error("Data partition not found");
    return std::nullopt;
  }

  // Round start UP and end DOWN to stay within partition bounds.
  uint32_t aligned_start =
      (partition_info->offset + FLASH_SECTOR_ALIGNMENT_MASK) &
      ~FLASH_SECTOR_ALIGNMENT_MASK;
  uint32_t partition_end = partition_info->offset + partition_info->size;
  uint32_t aligned_end = partition_end & ~FLASH_SECTOR_ALIGNMENT_MASK;

  if (aligned_start >= aligned_end) {
    logger.error("Partition too small after alignment, needs bytes: ",
                 static_cast<std::int32_t>(FLASH_SECTOR_SIZE));
    return std::nullopt;
  }

  logger.info("Original partition offset: ", partition_info->offset);
  logger.info("Original partition size: ", partition_info->size);
  logger.info("Aligned partition offset: ", aligned_start);
  logger.info("Aligned partition size: ", aligned_end - aligned_start);

  return StorageRegion{aligned_start, aligned_end - aligned_start};
}

} // namespace musin::filesystem
