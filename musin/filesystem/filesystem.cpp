// clang-format off
extern "C" {
#include "pico.h"  // Must be first to avoid platform.h error
#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h" // Include for vfs_get_lfs and PICO_FS_DEFAULT_SIZE
#include "hardware/flash.h" // Include for PICO_FLASH_SIZE_BYTES
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
}
// clang-format on

#include "filesystem.h"

namespace musin::filesystem {

FilesystemMount::FilesystemMount(filesystem_t *fs, const char *path,
                                 blockdevice_t *device, musin::Logger &logger)
    : fs_(fs), path_(path), mounted_(false), logger_(logger) {
  if (fs_ && path_ && device) {
    int err = fs_mount(path_, fs_, device);
    if (err == 0) {
      mounted_ = true;
      logger_.info("Filesystem mounted successfully");
    } else {
      logger_.error("Failed to mount filesystem");
    }
  }
}

FilesystemMount::~FilesystemMount() {
  if (mounted_ && fs_ && path_) {
    int err = fs_unmount(path_);
    if (err == 0) {
      logger_.info("Filesystem unmounted successfully");
    } else {
      logger_.error("Failed to unmount filesystem");
    }
    mounted_ = false;
  }
}

FilesystemMount::FilesystemMount(FilesystemMount &&other) noexcept
    : fs_(other.fs_), path_(other.path_), mounted_(other.mounted_),
      logger_(other.logger_) {
  other.fs_ = nullptr;
  other.path_ = nullptr;
  other.mounted_ = false;
}

FilesystemMount &FilesystemMount::operator=(FilesystemMount &&other) noexcept {
  if (this != &other) {
    // Clean up current state
    this->~FilesystemMount();

    // Move from other
    fs_ = other.fs_;
    path_ = other.path_;
    mounted_ = other.mounted_;

    // Reset other
    other.fs_ = nullptr;
    other.path_ = nullptr;
    other.mounted_ = false;
  }
  return *this;
}

Filesystem::Filesystem(musin::Logger &logger)
    : logger_(logger), fs_(nullptr), current_device_(nullptr) {
  // Create partition manager - it will handle detection internally
  // If partition detection fails, init() will fall back to legacy mode
  partition_manager_.emplace(logger);
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

  // Try partition approach first
  if (partition_manager_.has_value()) {
    auto data_partition = partition_manager_->find_data_partition();
    if (data_partition.has_value()) {
      logger_.info("Using partition-based filesystem");
      return init_with_partition(*data_partition);
    } else {
      logger_.warn("PartitionManager failed to find data partition");
    }
  }

  // Fall back to legacy approach
  logger_.info("Using legacy filesystem layout (no partitions)");
  return init_legacy();
}

bool Filesystem::init_with_partition(const PartitionInfo &partition) {
  logger_.info("Found Data partition. Offset: ", partition.offset);
  logger_.info("Data partition size: ", partition.size);

  // Use the standard flash block device which now handles partition access
  // correctly with XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE on RP2350
  current_device_ = blockdevice_flash_create(partition.offset, partition.size);

  if (!current_device_) {
    logger_.error("Failed to create flash block device for partition.");
    return false;
  }

  return mount_filesystem(current_device_);
}

bool Filesystem::init_legacy() {
  uint32_t filesystem_offset = PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE;
  uint32_t filesystem_size = PICO_FS_DEFAULT_SIZE;

  logger_.info("Legacy filesystem offset: ", filesystem_offset);
  logger_.info("Legacy filesystem size: ", filesystem_size);

  current_device_ =
      blockdevice_flash_create(filesystem_offset, filesystem_size);

  if (!current_device_) {
    logger_.error("Failed to create flash block device for legacy filesystem.");
    return false;
  }

  return mount_filesystem(current_device_);
}

bool Filesystem::mount_filesystem(blockdevice_t *flash) {
  fs_ = filesystem_littlefs_create(500, 16);

  // Try to mount using RAII FilesystemMount
  mount_.emplace(fs_, "/", flash, logger_);

  if (!mount_->is_mounted()) {
    logger_.warn("Initial mount failed, attempting to format");
    if (format_filesystem(flash)) {
      // Try mounting again after format
      mount_.emplace(fs_, "/", flash, logger_);
      return mount_->is_mounted();
    }
    return false;
  }

  return true;
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

bool Filesystem::format() {
  if (!current_device_) {
    logger_.error("No device available for formatting. Call init() first.");
    return false;
  }

  if (!fs_) {
    fs_ = filesystem_littlefs_create(500, 16);
  }

  return format_filesystem(current_device_);
}

} // namespace musin::filesystem