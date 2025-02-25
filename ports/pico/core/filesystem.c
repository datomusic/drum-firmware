#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"
#include <hardware/flash.h>
#include <stdio.h>
#include <string.h>
#include "pico/bootrom.h"
#include "boot/picobin.h"


bool init_filesystem(bool force_format) {
  /*
  printf("Adding partition with all permissions\n");
  int ret = rom_add_flash_runtime_partition(0, 0x1000,
                                        PICOBIN_PARTITION_PERMISSIONS_BITS);
  if (ret != 0) {
    printf("ERROR: Failed to add runtime partition\n");
  }
  */

  printf("init_filesystem, force_format: %b\n", force_format);
  blockdevice_t *flash =
      blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);

  printf("Erasing flash\n");
  int res = flash->erase(flash, 0, PICO_FS_DEFAULT_SIZE);
  if (res != BD_ERROR_OK) {
    printf("Error erasing flash: %i\n", res);
    // return false;
  }


  filesystem_t *lfs = filesystem_littlefs_create(500, 64);

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
