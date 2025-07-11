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
}

#include "filesystem.h"

namespace musin::filesystem {

static filesystem_t *g_fs = nullptr;

static bool format_filesystem(blockdevice_t *flash) {
  printf("Formatting filesystem with littlefs\n");
  int err = fs_format(g_fs, flash);
  if (err == -1) {
    printf("fs_format error: %s\n", strerror(errno));
    return false;
  }
  // Mount after formatting is essential
  printf("Mounting filesystem after format\n");
  err = fs_mount("/", g_fs, flash);
  if (err == -1) {
    printf("fs_mount after format error: %s\n", strerror(errno));
    return false;
  }
  return true;
}

void list_files(const char *path) {
  printf("Listing files in '%s':\n", path);
  DIR *dir = opendir(path);
  if (!dir) {
    printf("  Error opening directory: %s\n", strerror(errno));
    return;
  }

  struct dirent *dirent;
  while ((dirent = readdir(dir)) != NULL) {
    printf("  - %s\n", dirent->d_name);
  }

  int err = closedir(dir);
  if (err != 0) {
    printf("  Error closing directory: %s\n", strerror(errno));
  }
  printf("\n"); // Add a newline for better log separation
}

bool init(bool force_format) {
  printf("init_filesystem, force_format: %d\n", force_format);
  blockdevice_t *flash =
      blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
  g_fs = filesystem_littlefs_create(500, 16);

  if (force_format) {
    return format_filesystem(flash);
  } else {
    printf("Attempting to mount filesystem\n");
    int err = fs_mount("/", g_fs, flash);
    if (err == -1) {
      printf("Initial mount failed: %s. Attempting to format...\n",
             strerror(errno));
      return format_filesystem(flash);
    }
    return true; // Mount successful
  }
}

StorageInfo get_storage_info() {
  if (!g_fs || g_fs->type != FILESYSTEM_TYPE_LITTLEFS) {
    return {0, 0}; // Not a valid LittleFS filesystem
  }

  lfs_t *lfs = (lfs_t *)g_fs->context;
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
