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
   * This function attempts to mount the existing filesystem. If `force_format`
   * is true, it will format the filesystem before attempting to mount.
   *
   * @param force_format If true, the filesystem will be formatted even if
   * mounting an existing one could succeed.
   * @return true if the filesystem is successfully initialized (mounted), false
   * otherwise.
   */
  bool init(bool force_format);

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

private:
  std::optional<PartitionManager> partition_manager_;
  musin::Logger &logger_;
  filesystem_t *fs_;
  std::optional<FilesystemMount> mount_;

  bool format_filesystem(blockdevice_t *flash);
  bool init_with_partition(const PartitionInfo &partition, bool force_format);
  bool init_legacy(bool force_format);
  bool mount_filesystem(blockdevice_t *flash, bool force_format);
};

} // namespace musin::filesystem

#endif /* end of include guard: FILESYSTEM_H_A1PWKQIM */