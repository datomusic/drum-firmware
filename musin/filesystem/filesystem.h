#ifndef FILESYSTEM_H_A1PWKQIM
#define FILESYSTEM_H_A1PWKQIM

#include "musin/hal/logger.h"
#include "partition_manager.h"
#include <cstdint>
#include <optional>

// Forward declarations for filesystem types
typedef struct filesystem filesystem_t;
typedef struct blockdevice blockdevice_t;

namespace musin::filesystem {

struct StorageInfo {
  uint32_t total_bytes;
  uint32_t free_bytes;
};

class FilesystemMount {
public:
  FilesystemMount(filesystem_t *fs, const char *path, blockdevice_t *device,
                  musin::Logger &logger);
  ~FilesystemMount();

  // Non-copyable, movable
  FilesystemMount(const FilesystemMount &) = delete;
  FilesystemMount &operator=(const FilesystemMount &) = delete;
  FilesystemMount(FilesystemMount &&other) noexcept;
  FilesystemMount &operator=(FilesystemMount &&other) noexcept;

  bool is_mounted() const noexcept {
    return mounted_;
  }

private:
  filesystem_t *fs_;
  const char *path_;
  bool mounted_;
  musin::Logger &logger_;
};

class Filesystem {
public:
  explicit Filesystem(musin::Logger &logger);

  /**
   * @brief Initializes the filesystem.
   *
   * This function attempts to mount the existing filesystem. If mounting fails,
   * it will attempt to format and then mount.
   *
   * @return true if the filesystem is successfully initialized (mounted), false
   * otherwise.
   */
  bool init();

  /**
   * @brief Lists all files and directories at the given path.
   *
   * @param path The directory path to list (e.g., "/").
   */
  void list_files(const char *path);

  /**
   * @brief Gets the total and free space of the filesystem.
   *
   * @return A StorageInfo struct containing the total and free space in bytes.
   */
  StorageInfo get_storage_info();

  /**
   * @brief Explicitly formats the filesystem.
   *
   * This will format the filesystem, destroying any existing data.
   * Call init() after formatting to mount the formatted filesystem.
   *
   * @return true if formatting succeeds, false otherwise.
   */
  bool format();

private:
  std::optional<PartitionManager> partition_manager_;
  musin::Logger &logger_;
  filesystem_t *fs_;
  std::optional<FilesystemMount> mount_;

  bool format_filesystem(blockdevice_t *flash);
  bool init_with_partition(const PartitionInfo &partition);
  bool init_legacy();
  bool mount_filesystem(blockdevice_t *flash);
  blockdevice_t *current_device_;
};

} // namespace musin::filesystem

#endif /* end of include guard: FILESYSTEM_H_A1PWKQIM */