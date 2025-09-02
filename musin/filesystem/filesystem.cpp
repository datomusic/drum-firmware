// clang-format off
extern "C" {
#include "pico.h" // Must be included before bootrom_constants.h
#include "blockdevice/flash.h"
#include "boot/bootrom_constants.h" // Include for partition flags
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h" // Include for vfs_get_lfs
#include "hardware/regs/addressmap.h"
#include "pico/bootrom.h"
#include <dirent.h>
#include <errno.h>
#include <hardware/flash.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
}
// clang-format on

#include "filesystem.h"

namespace musin::filesystem {

Filesystem::Filesystem(musin::Logger &logger) : logger_(logger), fs_(nullptr) {
}

bool Filesystem::format_filesystem(blockdevice_t *flash) {
  logger_.info("Formatting filesystem with littlefs");
  int err = fs_format(fs_, flash);
  if (err == -1) {
    logger_.error("fs_format error");
    return false;
  }
  // Mount after formatting is essential
  logger_.info("Mounting filesystem after format");
  err = fs_mount("/", fs_, flash);
  if (err == -1) {
    logger_.error("fs_mount after format error");
    return false;
  }
  return true;
}

void Filesystem::list_files(const char *path) {
  logger_.info("Listing files in directory");
  DIR *dir = opendir(path);
  if (!dir) {
    logger_.error("Error opening directory");
    return;
  }

  struct dirent *dirent;
  while ((dirent = readdir(dir)) != NULL) {
    logger_.info("Found file");
  }

  int err = closedir(dir);
  if (err != 0) {
    logger_.error("Error closing directory");
  }
}

bool Filesystem::init() {
  logger_.info("Initializing filesystem");

  // --- Boot diagnostics to understand bootrom state ---
  uint32_t sys_info_buf[8];
  int sys_info_result = rom_get_sys_info(sys_info_buf, 8, 0);
  printf("PRINTF: rom_get_sys_info returned: %d\n", sys_info_result);
  if (sys_info_result > 0) {
    printf("PRINTF: sys_info_buf[0] = 0x%08lx\n", sys_info_buf[0]);
    printf("PRINTF: BOOT_DIAGNOSTIC_HAS_PARTITION_TABLE = 0x%08lx\n",
           (unsigned long)BOOT_DIAGNOSTIC_HAS_PARTITION_TABLE);
  }

  logger_.info("Boot diagnostics - get_sys_info returned: ",
               static_cast<std::int32_t>(sys_info_result));
  if (sys_info_result > 0 &&
      (sys_info_buf[0] & BOOT_DIAGNOSTIC_HAS_PARTITION_TABLE)) {
    logger_.info("Boot diagnostic: Partition table found");
  } else {
    logger_.warn("Boot diagnostic: No partition table found");
  }

  // --- Dynamically find the 'Data' partition ---
  static uint8_t
      pt_work_area[3264]; // Work area for bootrom partition functions
  printf("PRINTF: About to call rom_load_partition_table, work_area=%p, "
         "size=%zu\n",
         pt_work_area, sizeof(pt_work_area));

  std::int32_t rc =
      rom_load_partition_table(pt_work_area, sizeof(pt_work_area), true);
  printf("PRINTF: rom_load_partition_table returned: %ld\n", (long)rc);

  logger_.info("rom_load_partition_table returned: ", rc);
  if (rc < 0) {
    printf("PRINTF: rom_load_partition_table failed with error %ld\n",
           (long)rc);
    logger_.error("Failed to load partition table: ", rc);
    return false;
  }

  uint32_t data_partition_offset = 0;
  uint32_t data_partition_size = 0;

  // --- Get Data Partition (ID 2) Information ---
  const uint32_t data_partition_id = 2;
  uint32_t data_partition_info[8];
  // Flags to request location info for a single partition
  // PT_INFO_SINGLE_PARTITION is 0x8000, PT_INFO_PARTITION_LOCATION_AND_FLAGS is
  // 0x0010
  const uint32_t flags = 0x8000 | 0x0010 | data_partition_id;

  printf(
      "PRINTF: Getting info for Data partition (ID %ld) with flags 0x%08lx\n",
      (long)data_partition_id, (unsigned long)flags);
  int result = rom_get_partition_table_info(data_partition_info, 8, flags);
  printf(
      "PRINTF: rom_get_partition_table_info for Data partition returned: %d\n",
      result);

  if (result > 0) {
    // As per documentation, with PT_INFO_PARTITION_LOCATION_AND_FLAGS, the
    // buffer contains: word 0: supported_flags word 1: partition_location word
    // 2: partition_flags
    uint32_t supported_flags = data_partition_info[0];
    uint32_t partition_location = data_partition_info[1];
    uint32_t partition_flags = data_partition_info[2];

    printf("PRINTF: Data partition - supported_flags: 0x%08lx, location: "
           "0x%08lx, flags: 0x%08lx\n",
           (unsigned long)supported_flags, (unsigned long)partition_location,
           (unsigned long)partition_flags);

    // The physical offset is encoded in the lower 24 bits of the location.
    printf("PRINTF: Raw partition_location: 0x%08lx\n",
           (unsigned long)partition_location);
    uint32_t raw_offset = partition_location & 0xFFFFFF;
    // Align to flash sector boundary (4096 bytes)
    data_partition_offset =
        (raw_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    printf("PRINTF: Raw offset: 0x%08lx, aligned offset: 0x%08lx\n",
           (unsigned long)raw_offset, (unsigned long)data_partition_offset);
    // The size is known from the partition table JSON to be 4MB.
    data_partition_size = 0x400000;

    printf("PRINTF: Decoded Data partition - physical offset: 0x%08lx, size: "
           "0x%08lx\n",
           (unsigned long)data_partition_offset,
           (unsigned long)data_partition_size);
    logger_.info("Successfully found Data partition. Offset: ",
                 data_partition_offset);

  } else {
    printf("PRINTF: Failed to get info for Data partition, error: %d\n",
           result);
    logger_.error("Could not get Data partition info from bootrom: ",
                  static_cast<std::int32_t>(result));
  }

  if (data_partition_size == 0 || data_partition_offset == 0) {
    logger_.error(
        "Data partition not found or offset is zero, cannot initialize "
        "filesystem.");
    return false;
  }

  logger_.info("Initializing flash block device for Data partition.");
  printf("PRINTF: blockdevice_flash_create(offset=0x%08lx, "
         "size=0x%08lx)\n",
         (unsigned long)data_partition_offset,
         (unsigned long)data_partition_size);

  // Use the standard flash block device which now handles partition access
  // correctly with XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE on RP2350
  blockdevice_t *flash =
      blockdevice_flash_create(data_partition_offset, data_partition_size);

  if (!flash) {
    logger_.error("Failed to create flash block device.");
    return false;
  }

  fs_ = filesystem_littlefs_create(500, 16);

  logger_.info("Attempting to mount filesystem");
  int err = fs_mount("/", fs_, flash);
  if (err == -1) {
    logger_.warn("Initial mount failed, attempting to format");
    return format_filesystem(flash);
  }
  return true; // Mount successful
}

bool Filesystem::format() {
  logger_.info("Explicit format requested");

  // Reuse the same partition discovery logic from init()
  static uint8_t pt_work_area[3264];
  std::int32_t rc =
      rom_load_partition_table(pt_work_area, sizeof(pt_work_area), true);

  if (rc < 0) {
    logger_.error("Failed to load partition table for format: ", rc);
    return false;
  }

  const uint32_t data_partition_id = 2;
  uint32_t data_partition_info[8];
  const uint32_t flags = 0x8000 | 0x0010 | data_partition_id;

  int result = rom_get_partition_table_info(data_partition_info, 8, flags);
  if (result <= 0) {
    logger_.error("Could not get Data partition info for format: ",
                  static_cast<std::int32_t>(result));
    return false;
  }

  uint32_t partition_location = data_partition_info[1];
  uint32_t raw_offset = partition_location & 0xFFFFFF;
  uint32_t data_partition_offset =
      (raw_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
  uint32_t data_partition_size = 0x400000;

  blockdevice_t *flash =
      blockdevice_flash_create(data_partition_offset, data_partition_size);
  if (!flash) {
    logger_.error("Failed to create flash block device for format.");
    return false;
  }

  if (!fs_) {
    fs_ = filesystem_littlefs_create(500, 16);
  }

  return format_filesystem(flash);
}

StorageInfo Filesystem::get_storage_info() {
  if (!fs_ || fs_->type != FILESYSTEM_TYPE_LITTLEFS) {
    return {0, 0}; // Not a valid LittleFS filesystem
  }

  lfs_t *lfs = (lfs_t *)fs_->context;
  struct lfs_fsinfo info;
  int err = lfs_fs_stat(lfs, &info);
  if (err != 0) {
    return {0, 0};
  }

  uint32_t total_bytes = info.block_count * info.block_size;
  uint32_t used_bytes = lfs_fs_size(lfs) * info.block_size;
  uint32_t free_bytes = total_bytes - used_bytes;

  return {total_bytes, free_bytes};
}

} // namespace musin::filesystem