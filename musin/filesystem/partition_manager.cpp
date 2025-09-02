#include "partition_manager.h"

// clang-format off
extern "C" {
#include "pico.h"  // Must be first to avoid platform.h error
#include "boot/bootrom_constants.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
#include "pico/bootrom.h"
}
// clang-format on

#include <cstdio>

// Macros to extract partition location fields, based on ai/partition_info.c
#define PART_LOC_FIRST(x)                                                      \
  (((x) & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>                     \
   PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB)
#define PART_LOC_LAST(x)                                                       \
  (((x) & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>                      \
   PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB)

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

  // First get partition table info to check if it exists and get partition
  // count
  uint32_t pt_info[8];
  uint32_t flags = PT_INFO_PT_INFO | PT_INFO_PARTITION_LOCATION_AND_FLAGS |
                   PT_INFO_PARTITION_ID;
  int rc = rom_get_partition_table_info(pt_info, sizeof(pt_info), flags);

  if (rc < 0) {
    logger_.error("Failed to get partition table info: ",
                  static_cast<std::int32_t>(rc));
    partition_count_ = 0;
    has_partition_table_ = false;
    partition_table_loaded_ = true; // Mark as loaded even if empty
    return false;
  }

  // Parse partition table header
  [[maybe_unused]] uint32_t returned_fields = pt_info[0];
  partition_count_ = pt_info[1] & 0x000000FF;
  has_partition_table_ = (pt_info[1] & 0x00000100) != 0;

  logger_.info("Partition table loaded, partition count: ",
               static_cast<uint32_t>(partition_count_));

  // Load the full partition table into work area
  std::int32_t load_rc =
      rom_load_partition_table(pt_work_area_, sizeof(pt_work_area_), true);
  logger_.info("rom_load_partition_table returned: ", load_rc);

  if (load_rc < 0) {
    logger_.error("Failed to load partition table: ", load_rc);
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
  uint32_t first_sector = PART_LOC_FIRST(partition_location);
  uint32_t last_sector = PART_LOC_LAST(partition_location);
  uint32_t aligned_offset = first_sector * FLASH_SECTOR_SIZE;
  uint32_t partition_size =
      (last_sector - first_sector + 1) * FLASH_SECTOR_SIZE;

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

std::optional<PartitionInfo>
PartitionManager::find_partition_by_name(etl::string_view name) {
  if (!load_partition_table()) {
    return std::nullopt;
  }

  logger_.debug("Searching for partition by name");

  // Enumerate all partitions and find by name
  for (int i = 0; i < partition_count_; ++i) {
    auto partition_info = get_partition_info(static_cast<uint32_t>(i));
    if (partition_info.has_value() && partition_info->has_name) {
      etl::string_view partition_name = partition_info->get_name();
      if (partition_name == name) {
        logger_.info("Found partition by name at index: ",
                     static_cast<uint32_t>(i));
        return partition_info;
      }
    }
  }

  logger_.warn("Partition not found by name");
  return std::nullopt;
}

PartitionManager::PartitionList PartitionManager::list_all_partitions() {
  PartitionList partitions;

  if (!load_partition_table()) {
    logger_.error("Cannot list partitions: partition table not loaded");
    return partitions;
  }

  logger_.debug("Enumerating partitions, count: ",
                static_cast<uint32_t>(partition_count_));

  for (int i = 0; i < partition_count_; ++i) {
    auto partition_info = get_partition_info(static_cast<uint32_t>(i));
    if (partition_info.has_value()) {
      partitions.push_back(partition_info.value());
      logger_.debug("Added partition at index: ", static_cast<uint32_t>(i));
    } else {
      logger_.warn("Failed to get info for partition index ",
                   static_cast<uint32_t>(i));
    }
  }

  logger_.info("Successfully enumerated partitions, found: ",
               static_cast<uint32_t>(partitions.size()));
  return partitions;
}

bool PartitionManager::has_partition_table() const noexcept {
  return has_partition_table_;
}

int PartitionManager::get_partition_count() const noexcept {
  return partition_count_;
}

std::optional<PartitionInfo>
PartitionManager::get_partition_info(uint32_t partition_index) {
  if (!load_partition_table() ||
      partition_index >= static_cast<uint32_t>(partition_count_)) {
    return std::nullopt;
  }

  // Get basic partition info (location and flags)
  uint32_t basic_info[8];
  uint32_t flags = PT_INFO_SINGLE_PARTITION |
                   PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_PARTITION_ID;
  int rc = rom_get_partition_table_info(basic_info, sizeof(basic_info),
                                        (partition_index << 24) | flags);

  if (rc < 0) {
    logger_.error("Failed to get partition info for index ", partition_index);
    return std::nullopt;
  }

  PartitionInfo info{};

  // Parse location (word 1) - use proper macros from picobin.h
  uint32_t partition_location = basic_info[1];
  uint32_t first_sector = PART_LOC_FIRST(partition_location);
  uint32_t last_sector = PART_LOC_LAST(partition_location);

  info.offset = first_sector * FLASH_SECTOR_SIZE;
  info.size = (last_sector - first_sector + 1) * FLASH_SECTOR_SIZE;

  // Parse flags and permissions (word 2)
  info.flags_and_permissions = basic_info[2];
  bool has_id =
      (info.flags_and_permissions & PICOBIN_PARTITION_FLAGS_HAS_ID_BITS) != 0;
  bool has_name =
      (info.flags_and_permissions & PICOBIN_PARTITION_FLAGS_HAS_NAME_BITS) != 0;

  info.has_id = has_id;
  info.has_name = has_name;

  // Get partition ID if present
  if (has_id && rc >= 4) {
    uint32_t id_low = basic_info[3];
    uint32_t id_high = (rc >= 5) ? basic_info[4] : 0;
    info.partition_id = ((uint64_t)id_high << 32) | id_low;
  } else {
    info.partition_id = 0;
  }

  // Get name if present
  if (has_name) {
    uint32_t name_info[32]; // Buffer for name data
    uint32_t name_flags = PT_INFO_SINGLE_PARTITION | PT_INFO_PARTITION_NAME;
    int name_rc = rom_get_partition_table_info(
        name_info, sizeof(name_info), (partition_index << 24) | name_flags);

    if (name_rc > 0) {
      // Name is stored as: [fields][length_byte + name_data...]
      uint8_t *name_buf = reinterpret_cast<uint8_t *>(&name_info[1]);
      uint8_t name_length = *name_buf & 0x7F;
      name_buf++;

      if (name_length > 0 && name_length <= 127) {
        info.name.assign(reinterpret_cast<char *>(name_buf), name_length);
      }
    }
  }

  logger_.debug("Retrieved partition info for index: ", partition_index);
  return info;
}

} // namespace musin::filesystem
