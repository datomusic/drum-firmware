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

Filesystem::Filesystem(musin::Logger &logger)
    : logger_(logger), fs_(nullptr), partition_manager_(logger) {
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

  if (!partition_manager_.check_partition_table_available()) {
    logger_.error(
        "Partition table not available, cannot initialize filesystem");
    return false;
  }

  const uint32_t data_partition_id = 2;
  auto partition_info = partition_manager_.find_partition(data_partition_id);

  if (!partition_info) {
    logger_.error("Data partition not found, cannot initialize filesystem");
    return false;
  }

  blockdevice_t *flash =
      partition_manager_.create_partition_blockdevice(*partition_info);
  if (!flash) {
    logger_.error("Failed to create flash block device");
    return false;
  }

  fs_ = filesystem_littlefs_create(500, 16);

  logger_.info("Attempting to mount filesystem");
  int err = fs_mount("/", fs_, flash);
  if (err == -1) {
    logger_.warn("Initial mount failed, attempting to format");
    return format_filesystem(flash);
  }
  return true;
}

bool Filesystem::format() {
  logger_.info("Explicit format requested");

  const uint32_t data_partition_id = 2;
  auto partition_info = partition_manager_.find_partition(data_partition_id);

  if (!partition_info) {
    logger_.error("Data partition not found for format");
    return false;
  }

  blockdevice_t *flash =
      partition_manager_.create_partition_blockdevice(*partition_info);
  if (!flash) {
    logger_.error("Failed to create flash block device for format");
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