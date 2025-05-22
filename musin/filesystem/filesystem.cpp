extern "C" {
#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"
#include <hardware/flash.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
}

namespace musin::filesystem {

static bool format_filesystem_internal(filesystem_t *lfs, blockdevice_t *flash) {
  printf("Formatting filesystem with littlefs\n");
  int err = fs_format(lfs, flash);
  if (err == -1) {
    printf("fs_format error: %s\n", strerror(errno));
    return false;
  }
  // Mount after formatting is essential
  printf("Mounting filesystem after format\n");
  err = fs_mount("/", lfs, flash);
  if (err == -1) {
    printf("fs_mount after format error: %s\n", strerror(errno));
    return false;
  }
  return true;
}

bool init(bool force_format) {
  printf("init_filesystem, force_format: %d\n", force_format);
  blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
  filesystem_t *lfs = filesystem_littlefs_create(500, 16);

  if (force_format) {
    return format_filesystem_internal(lfs, flash);
  } else {
    printf("Attempting to mount filesystem\n");
    int err = fs_mount("/", lfs, flash);
    if (err == -1) {
      printf("fs_mount error: %s\n", strerror(errno));
      return false;
    }
    return true; // Mount successful
  }
}

} // namespace musin::filesystem
