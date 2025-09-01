extern "C" {
#include <dirent.h>
#include <errno.h>
#include <hardware/flash.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h" // Include for vfs_get_lfs
#include "pico/bootrom.h"
#include "boot/bootrom_constants.h" // Include for partition flags
}

#include "filesystem.h"

namespace musin::filesystem {

Filesystem::Filesystem(musin::Logger& logger) : logger_(logger), fs_(nullptr) {}

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
  logger_.info("Initializing filesystem, force_format: ", static_cast<uint32_t>(force_format));

  // --- Dynamically find the 'Data' partition ---
  static uint8_t pt_work_area[3264]; // Work area for bootrom partition functions
  std::int32_t rc = rom_load_partition_table(pt_work_area, sizeof(pt_work_area), false);
  if (rc < 0) {
      logger_.error("Failed to load partition table: ", rc);
      return false;
  }

  uint32_t data_partition_offset = 0;
  uint32_t data_partition_size = 0;

  for (uint32_t i = 0; i < PARTITION_TABLE_MAX_PARTITIONS; ++i) {
      uint32_t name_buf[10]; // Buffer for partition name
      int words_written = rom_get_partition_table_info(name_buf, count_of(name_buf), i | PT_INFO_PARTITION_NAME);

      if (words_written > 1 && (name_buf[0] & PT_INFO_PARTITION_NAME)) {
          uint8_t *name_payload = (uint8_t *)&name_buf[1];
          uint8_t name_len = name_payload[0];
          char *part_name = (char *)&name_payload[1];

          logger_.debug("Found partition: ", i);

          if (name_len == 4 && strncmp(part_name, "Data", 4) == 0) {
              // Found "Data" partition. Now get its location and size.
              uint32_t loc_buf[3]; // Expecting flags, offset, size
              words_written = rom_get_partition_table_info(loc_buf, count_of(loc_buf), i | PT_INFO_PARTITION_LOCATION_AND_FLAGS);

              if (words_written == 3 && (loc_buf[0] & PT_INFO_PARTITION_LOCATION_AND_FLAGS)) {
                  data_partition_offset = loc_buf[1];
                  data_partition_size = loc_buf[2];
                  break; // Found what we need, exit loop
              }
          }
      } else if (words_written < 0) {
          // An error occurred, or we've iterated past the last partition
          break;
      }
  }

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