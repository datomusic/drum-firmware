#ifndef FILESYSTEM_H_A1PWKQIM
#define FILESYSTEM_H_A1PWKQIM

namespace musin::filesystem {

// Forward declarations for opaque C types used by the filesystem
struct filesystem_t;
struct blockdevice_t;

/**
 * @brief Initializes the filesystem.
 *
 * This function attempts to mount the existing filesystem. If `force_format` is true,
 * it will format the filesystem before attempting to mount.
 *
 * @param force_format If true, the filesystem will be formatted even if mounting an existing one could succeed.
 * @return true if the filesystem is successfully initialized (mounted), false otherwise.
 */
bool init(bool force_format);

/**
 * @brief Formats the filesystem and then mounts it.
 *
 * This function should be called if a fresh filesystem is required or if the existing
 * filesystem is corrupted.
 *
 * @param lfs Pointer to the filesystem_t structure.
 * @param flash Pointer to the blockdevice_t structure for the flash memory.
 * @return true if formatting and subsequent mounting are successful, false otherwise.
 */
bool format_filesystem(filesystem_t *lfs, blockdevice_t *flash);

}

#endif /* end of include guard: FILESYSTEM_H_A1PWKQIM */
