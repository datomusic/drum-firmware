extern "C" {
#include <dirent.h>
#include <errno.h>
#include <hardware/flash.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "blockdevice/flash.h"
#include "boot/bootrom_constants.h" // Include for partition flags
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h" // Include for vfs_get_lfs
#include "pico/bootrom.h"
}

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

bool Filesystem::init(bool force_format) {
  logger_.info("Initializing filesystem, force_format: ",
               static_cast<uint32_t>(force_format));

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

  // --- Try using rom_pick_ab_partition to get partition info ---
  printf("PRINTF: Trying rom_pick_ab_partition approach\n");
  static uint8_t pick_work_area[3264]; // Work area for pick_ab_partition

  // Try to use pick_ab_partition to get info about partition 0 (which should
  // also tell us about partition 2)
  int picked_partition =
      rom_pick_ab_partition(pick_work_area, sizeof(pick_work_area), 0, 0);
  printf("PRINTF: rom_pick_ab_partition returned: %d\n", picked_partition);

  if (picked_partition >= 0) {
    printf("PRINTF: Successfully picked partition %d\n", picked_partition);
    printf("PRINTF: Currently running from: %s\n",
           picked_partition == 0 ? "Firmware A" : "Firmware B");
    logger_.info("Partition system is working, but need alternative approach "
                 "for Data partition");
    logger_.info("Currently running from partition: ",
                 static_cast<std::int32_t>(picked_partition));

    // Compare with working firmware partition to understand the structure
    uint32_t firmware_partition_info[8];
    printf("PRINTF: Getting info for current firmware partition %d for "
           "comparison\n",
           picked_partition);
    int firmware_info_result = rom_get_partition_table_info(
        firmware_partition_info, 8, picked_partition);
    printf("PRINTF: rom_get_partition_table_info(partition %d) returned: %d\n",
           picked_partition, firmware_info_result);

    if (firmware_info_result > 0) {
      printf("PRINTF: Firmware partition %d info:\n", picked_partition);
      for (int i = 0; i < 8; i++) {
        printf("PRINTF: firmware_partition_info[%d] = 0x%08lx\n", i,
               (unsigned long)firmware_partition_info[i]);
      }
    }

    // Try to get Data partition (partition 2) location info using proper flags
    uint32_t data_partition_info[8];
    // Request: SINGLE_PARTITION (0x8000) | PARTITION_LOCATION_AND_FLAGS
    // (0x0010) | partition_id (2)
    uint32_t flags = 0x8000 | 0x0010 |
                     2; // PT_INFO_SINGLE_PARTITION |
                        // PT_INFO_PARTITION_LOCATION_AND_FLAGS | partition 2
    printf("PRINTF: Attempting to get Data partition location (partition 2, "
           "flags=0x%04lx)\n",
           (unsigned long)flags);
    int data_info_result =
        rom_get_partition_table_info(data_partition_info, 8, flags);
    printf("PRINTF: rom_get_partition_table_info(partition 2, location flags) "
           "returned: %d\n",
           data_info_result);

    if (data_info_result > 0) {
      printf(
          "PRINTF: Data partition location info success! Words returned: %d\n",
          data_info_result);
      printf("PRINTF: Supported flags: 0x%08lx\n",
             (unsigned long)data_partition_info[0]);

      // According to bootrom_constants.h, PT_INFO_PARTITION_LOCATION_AND_FLAGS
      // returns: 2 words: partition_location, partition_flags
      if (data_info_result >= 3) { // flags word + 2 data words
        uint32_t partition_location = data_partition_info[1];
        uint32_t partition_flags = data_partition_info[2];
        printf(
            "PRINTF: partition_location = 0x%08lx, partition_flags = 0x%08lx\n",
            (unsigned long)partition_location, (unsigned long)partition_flags);

        // Location likely encodes both offset and size - let's decode it
        // From picotool: Data partition is 0x202000->0x602000 (4MB)
        uint32_t expected_start = 0x202000;
        uint32_t expected_end = 0x602000;
        printf("PRINTF: Expected: start=0x%08lx, end=0x%08lx, size=0x%08lx\n",
               (unsigned long)expected_start, (unsigned long)expected_end,
               (unsigned long)(expected_end - expected_start));

        // Try to decode partition_location - it seems to encode both offset and
        // size partition_location = 0xfc202002, expected start = 0x202000 Let's
        // try different decoding approaches:

        // Approach 1: Lower 24 bits = offset, upper 8 bits = size encoding
        uint32_t decoded_offset_1 = partition_location & 0xFFFFFF;
        uint32_t size_encoding_1 = (partition_location >> 24) & 0xFF;
        printf("PRINTF: Decode 1 - offset: 0x%06lx, size_encoding: 0x%02lx\n",
               (unsigned long)decoded_offset_1, (unsigned long)size_encoding_1);

        // Approach 2: Lower 20 bits = offset in sectors, next 12 bits = size in
        // sectors
        uint32_t offset_sectors_2 = partition_location & 0xFFFFF;
        uint32_t size_sectors_2 = (partition_location >> 20) & 0xFFF;
        uint32_t decoded_offset_2 = offset_sectors_2 * 0x1000; // 4K sectors
        uint32_t decoded_size_2 = size_sectors_2 * 0x1000;
        printf("PRINTF: Decode 2 - offset: 0x%08lx, size: 0x%08lx (sectors: "
               "%ld, %ld)\n",
               (unsigned long)decoded_offset_2, (unsigned long)decoded_size_2,
               (unsigned long)offset_sectors_2, (unsigned long)size_sectors_2);

        // The bootrom gave us 0x202002, which is very close to expected
        // 0x202000 Let's try using the bootrom's exact offset value since
        // hardcoding 0x202000 crashed
        if (decoded_offset_1 >= 0x202000 && decoded_offset_1 <= 0x202010) {
          printf("PRINTF: Using bootrom's exact offset: 0x%08lx (decode 1)\n",
                 (unsigned long)decoded_offset_1);
          data_partition_offset = decoded_offset_1;
          // For size, let's use the known 4MB from our partition table
          data_partition_size = 0x400000;
          printf("PRINTF: Setting filesystem: offset=0x%08lx, size=0x%08lx\n",
                 (unsigned long)data_partition_offset,
                 (unsigned long)data_partition_size);
        } else if (decoded_offset_2 == expected_start) {
          printf(
              "PRINTF: SUCCESS! Decode 2 matches expected offset exactly!\n");
          data_partition_offset = decoded_offset_2;
          data_partition_size = decoded_size_2;
          printf("PRINTF: Setting filesystem: offset=0x%08lx, size=0x%08lx\n",
                 (unsigned long)data_partition_offset,
                 (unsigned long)data_partition_size);
        } else {
          printf("PRINTF: No decoding approach matched expected values\n");
        }
      }

      // Show all returned data for analysis
      for (int i = 0; i < data_info_result && i < 8; i++) {
        printf("PRINTF: data_partition_info[%d] = 0x%08lx\n", i,
               (unsigned long)data_partition_info[i]);
      }
    } else {
      printf("PRINTF: Data partition info failed with error: %d\n",
             data_info_result);
    }
    // TODO: Implement alternative approach for Data partition access
  } else {
    printf("PRINTF: rom_pick_ab_partition failed with: %d\n", picked_partition);
    logger_.error("All bootrom partition APIs failed, filesystem unavailable");
  }

  // CRITICAL: ANY attempt to access Data partition causes device hangs
  // This includes both 0x202000 and 0x202002 addresses from bootrom
  // Disable filesystem until root cause is found
  logger_.warn(
      "Filesystem disabled - Data partition access causes device hangs");
  return true; // Return success to allow device to boot without filesystem

  if (data_partition_size == 0) {
    logger_.error("Data partition not found or could not read size/offset");
    return false;
  }

  logger_.info("Found Data partition at offset: ", data_partition_offset);
  logger_.info("Data partition size: ", data_partition_size);

  blockdevice_t *flash =
      blockdevice_flash_create(data_partition_offset, data_partition_size);
  // --- End of dynamic find ---

  fs_ = filesystem_littlefs_create(500, 16);

  if (force_format) {
    return format_filesystem(flash);
  } else {
    logger_.info("Attempting to mount filesystem");
    int err = fs_mount("/", fs_, flash);
    if (err == -1) {
      logger_.warn("Initial mount failed, attempting to format");
      return format_filesystem(flash);
    }
    return true; // Mount successful
  }
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