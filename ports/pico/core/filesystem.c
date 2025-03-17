#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"
#include <hardware/flash.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool init_filesystem(bool force_format) {
  printf("init_filesystem, force_format: %b\n", force_format);
  blockdevice_t *flash =
      blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
  filesystem_t *lfs = filesystem_littlefs_create(500, 16);

  printf("Mounting filesystem\n");
  int err = fs_mount("/", lfs, flash);
  if (force_format || err == -1) {
    printf("format / with littlefs\n");
    err = fs_format(lfs, flash);
    if (err == -1) {
      printf("fs_format error: %s\n", strerror(errno));
      return false;
    }
    err = fs_mount("/", lfs, flash);
    if (err == -1) {
      printf("fs_mount error: %s\n", strerror(errno));
      return false;
    }
  }

  return err == 0;
}
