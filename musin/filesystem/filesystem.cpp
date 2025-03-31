#include "../logging.h"
#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"
#include <hardware/flash.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

using Musin::Logging::log;
using Musin::Logging::Info;

namespace Musin::Filesystem {

bool init(bool force_format) {
  log::info_f("%s %b %s %i", "init_filesystem, force_format:", force_format, "other", 9999);
  blockdevice_t *flash =
      blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
  filesystem_t *lfs = filesystem_littlefs_create(500, 16);

  log::info("Mounting filesystem");
  int err = fs_mount("/", lfs, flash);
  if (force_format || err == -1) {
    log::info("format / with littlefs");
    err = fs_format(lfs, flash);
    if (err == -1) {
      log::error_f("fs_format error: %s", strerror(errno));
      return false;
    }
    err = fs_mount("/", lfs, flash);
    if (err == -1) {
      log::error_f("fs_mount error: %s", strerror(errno));
      return false;
    }
  }

  return err == 0;
}

} // namespace Musin::Filesystem
