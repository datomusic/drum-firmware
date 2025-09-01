#include "partition_manager.h"

extern "C" {
#include "boot/bootrom_constants.h"
#include "hardware/flash.h"
#include "pico.h"
#include "pico/bootrom.h"
}

#include <cstdio>

namespace musin::filesystem {

// Define the static work area for bootrom functions
// This moves the large buffer from the stack to the .bss section
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
uint8_t PartitionManager::pt_work_area_[PT_WORK_AREA_SIZE];

PartitionManager::PartitionManager(musin::Logger &logger) : logger_(logger) {
}

void PartitionManager::log_boot_diagnostics() {
  uint32_t sys_info_buf[8];
  int sys_info_result = rom_get_sys_info(sys_info_buf, 8, 0);
  logger_.info("Boot diagnostics - get_sys_info returned: ",
               static_cast<std::int32_t>(sys_info_result));
  if (sys_info_result > 0 &&
      (sys_info_buf[0] & BOOT_DIAGNOSTIC_HAS_PARTITION_TABLE)) {
    logger_.info("Boot diagnostic: Partition table found");
  } else {
    logger_.warn("Boot diagnostic: No partition table found");
  }
}

bool PartitionManager::load_partition_table() {
  if (partition_table_loaded_) {
    return true;
  }

  log_boot_diagnostics();

  logger_.debug("Loading partition table");

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

std::optional<PartitionInfo> PartitionManager::find_data_partition() {
  return find_partition_by_id(2); // Data partition is ID 2
}

std::optional<PartitionInfo>
PartitionManager::find_partition_by_id(uint32_t partition_id) {
  if (!load_partition_table()) {
    return std::nullopt;
  }

  uint32_t partition_info[8];
  const uint32_t flags = PT_INFO_SINGLE_PARTITION |
                         PT_INFO_PARTITION_LOCATION_AND_FLAGS | partition_id;

  logger_.debug("Getting info for partition ID: ", partition_id);

  int result = rom_get_partition_table_info(partition_info, 8, flags);

  if (result <= 0) {
    logger_.error("Could not get partition info from bootrom: ",
                  static_cast<std::int32_t>(result));
    return std::nullopt;
  }

  uint32_t partition_location = partition_info[1];
  uint32_t raw_offset = partition_location & 0xFFFFFF;
  uint32_t aligned_offset =
      (raw_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;

  // TODO: Calculate actual size from partition table instead of hardcoding
  uint32_t partition_size = 0x400000; // Temporary 4MB assumption

  logger_.info("Found partition ID ", partition_id);
  logger_.info("  Offset: ", aligned_offset);
  logger_.info("  Size: ", partition_size);

  if (partition_size == 0 || aligned_offset == 0) {
    logger_.error("Partition has zero offset or size.");
    return std::nullopt;
  }

  PartitionInfo info{};
  info.offset = aligned_offset;
  info.size = partition_size;
  info.flags_and_permissions = partition_info[2];
  info.partition_id = partition_id;
  info.has_id = true;
  info.has_name = false; // TODO: Implement name lookup

  return info;
}

std::optional<PartitionInfo> PartitionManager::find_partition_by_name(
    [[maybe_unused]] etl::string_view name) {
  // TODO: Implement name-based partition discovery
  return std::nullopt;
}

PartitionManager::PartitionList PartitionManager::list_all_partitions() {
  PartitionList partitions;
  // TODO: Implement full partition enumeration
  return partitions;
}

bool PartitionManager::has_partition_table() const noexcept {
  return has_partition_table_;
}

int PartitionManager::get_partition_count() const noexcept {
  return partition_count_;
}

std::optional<PartitionInfo> PartitionManager::get_partition_info(
    [[maybe_unused]] uint32_t partition_index) {
  // TODO: Implement individual partition info retrieval
  return std::nullopt;
}

} // namespace musin::filesystem
