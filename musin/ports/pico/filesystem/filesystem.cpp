// clang-format off
extern "C" {
#include "pico.h"
#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h" // Include for vfs_get_lfs
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
}
// clang-format on

#include "musin/filesystem/filesystem.h"
#include "audio_safe_flash.h"
#include "musin/filesystem/storage_region.h"

namespace {

blockdevice_t *
create_storage_blockdevice(const musin::filesystem::StorageRegion &region,
                           musin::Logger &logger) {
  blockdevice_t *flash = blockdevice_flash_create(region.offset, region.size);
  if (!flash) {
    logger.error("Failed to create flash block device");
    return nullptr;
  }

  // Keep the audio interrupt alive during erase/program so persistence
  // writes do not glitch playback.
  musin::filesystem::install_flash_guard(
      flash, region.offset, musin::filesystem::audio_priority_flash_guard());

  return flash;
}

} // namespace

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

  logger_.info("Step 1: Locating data storage region");
  auto region = get_storage_region(logger_);
  if (!region) {
    logger_.error("Storage region not found, cannot initialize filesystem");
    return false;
  }

  logger_.info("Step 2: Storage region found, creating block device");
  blockdevice_t *flash = create_storage_blockdevice(*region, logger_);
  if (!flash) {
    return false;
  }

  logger_.info("Step 3: Creating littlefs filesystem instance");
  fs_ = filesystem_littlefs_create(500, 16);
  if (!fs_) {
    logger_.error("Failed to create littlefs instance");
    return false;
  }

  logger_.info("Step 4: Attempting to mount filesystem");
  int err = fs_mount("/", fs_, flash);
  logger_.info("fs_mount returned: ", static_cast<std::int32_t>(err));

  if (err == -1) {
    logger_.warn("Initial mount failed, attempting to format");
    return format_filesystem(flash);
  }

  logger_.info("Filesystem mounted successfully");
  return true;
}

bool Filesystem::format() {
  logger_.info("Explicit format requested");

  logger_.info("Format Step 1: Locating data storage region");
  auto region = get_storage_region(logger_);
  if (!region) {
    logger_.error("Storage region not found for format");
    return false;
  }

  logger_.info("Format Step 2: Creating block device for format");
  blockdevice_t *flash = create_storage_blockdevice(*region, logger_);
  if (!flash) {
    return false;
  }

  logger_.info("Format Step 3: Ensuring filesystem instance exists");
  if (!fs_) {
    logger_.info("Creating new filesystem instance for format");
    fs_ = filesystem_littlefs_create(500, 16);
    if (!fs_) {
      logger_.error("Failed to create filesystem instance for format");
      return false;
    }
  } else {
    logger_.info("Using existing filesystem instance for format");
  }

  logger_.info("Format Step 4: Executing filesystem format");
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
