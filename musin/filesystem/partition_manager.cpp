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

uint8_t PartitionManager::pt_work_area_[3264];

PartitionManager::PartitionManager(musin::Logger &logger)
    : logger_(logger), partition_table_loaded_(false) {
}

bool PartitionManager::check_partition_table_available() {
  uint32_t sys_info_buf[8];
  int sys_info_result = rom_get_sys_info(sys_info_buf, 8, 0);

  logger_.info("Boot diagnostics - get_sys_info returned: ",
               static_cast<std::int32_t>(sys_info_result));

  if (sys_info_result > 0 &&
      (sys_info_buf[0] & BOOT_DIAGNOSTIC_HAS_PARTITION_TABLE)) {
    logger_.info("Boot diagnostic: Partition table found");
    return true;
  }

  logger_.warn("Boot diagnostic: No partition table found");
  return false;
}

bool PartitionManager::load_partition_table() {
  if (partition_table_loaded_) {
    return true;
  }

  std::int32_t rc =
      rom_load_partition_table(pt_work_area_, sizeof(pt_work_area_), true);
  logger_.info("rom_load_partition_table returned: ", rc);

  if (rc < 0) {
    logger_.error("Failed to load partition table: ", rc);
    return false;
  }

  partition_table_loaded_ = true;
  return true;
}

std::optional<PartitionInfo>
PartitionManager::find_partition(uint32_t partition_id) {
  if (!load_partition_table()) {
    return std::nullopt;
  }

  uint32_t partition_info[8];
  const uint32_t flags = 0x8000 | 0x0010 | partition_id;

  int result = rom_get_partition_table_info(partition_info, 8, flags);

  if (result <= 0) {
    logger_.error("Could not get partition info for ID: ",
                  static_cast<std::int32_t>(partition_id));
    return std::nullopt;
  }

  uint32_t partition_location = partition_info[1];
  uint32_t partition_flags = partition_info[2];

  uint32_t first_sector = PART_LOC_FIRST(partition_location);
  uint32_t last_sector = PART_LOC_LAST(partition_location);

  uint32_t offset = first_sector * FLASH_SECTOR_SIZE;
  uint32_t size = (last_sector + 1 - first_sector) * FLASH_SECTOR_SIZE;

  logger_.info("Found partition ID ", static_cast<std::int32_t>(partition_id));
  logger_.info("Partition offset: ", offset);
  logger_.info("Partition size: ", size);

  return PartitionInfo{offset, size, partition_flags};
}

blockdevice_t *PartitionManager::create_partition_blockdevice(
    const PartitionInfo &partition_info) {
  logger_.info("Creating block device for partition");

  blockdevice_t *flash =
      blockdevice_flash_create(partition_info.offset, partition_info.size);

  if (!flash) {
    logger_.error("Failed to create flash block device");
  }

  return flash;
}

} // namespace musin::filesystem