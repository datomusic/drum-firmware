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

  // --- Dynamically find the 'Data' partition ---
  static uint8_t pt_work_area[3264]; // Work area for bootrom partition functions
  int rc = rom_load_partition_table(pt_work_area, sizeof(pt_work_area), false);
  if (rc < 0) {
      printf("Error: failed to load partition table (%d)\n", rc);
      return false;
  }

  uint32_t data_partition_offset = 0;
  uint32_t data_partition_size = 0;

  for (uint8_t i = 0; i < PARTITION_TABLE_MAX_PARTITIONS; ++i) {
      uint32_t name_buf[10]; // Buffer for partition name
      int words_written = rom_get_partition_table_info(name_buf, count_of(name_buf), i | PT_INFO_PARTITION_NAME);

      if (words_written > 1 && (name_buf[0] & PT_INFO_PARTITION_NAME)) {
          uint8_t *name_payload = (uint8_t *)&name_buf[1];
          uint8_t name_len = name_payload[0];
          char *part_name = (char *)&name_payload[1];

          printf("Found partition %d: '%.*s'\n", i, name_len, part_name);

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
      printf("Error: 'Data' partition not found or could not read size/offset.\n");
      return false;
  }

  printf("Found 'Data' partition at offset 0x%x with size 0x%x\n",
         (unsigned int)data_partition_offset, (unsigned int)data_partition_size);

  blockdevice_t *flash =
      blockdevice_flash_create(data_partition_offset, data_partition_size);
  // --- End of dynamic find ---

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