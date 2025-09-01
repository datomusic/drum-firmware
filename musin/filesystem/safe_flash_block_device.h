#ifndef SAFE_FLASH_BLOCK_DEVICE_H_
#define SAFE_FLASH_BLOCK_DEVICE_H_

#include <cstdint>

extern "C" {
#include "blockdevice/flash.h"
}

namespace musin::filesystem {

/**
 * @brief Creates a flash block device that can safely access partitions
 * outside the booted area using the untranslated XIP window.
 *
 * This is a wrapper around the standard flash block device that handles
 * partition access correctly when the partition is outside the current
 * booted partition's XIP mapping.
 *
 * @param flash_offset The flash offset (not XIP address) where the partition
 * starts
 * @param size Size of the partition in bytes
 * @return blockdevice_t* Pointer to the created block device, or nullptr on
 * failure
 */
blockdevice_t *safe_flash_block_device_create(uint32_t flash_offset,
                                              size_t size);

/**
 * @brief Frees a safe flash block device created with
 * safe_flash_block_device_create
 *
 * @param device The block device to free
 */
void safe_flash_block_device_free(blockdevice_t *device);

} // namespace musin::filesystem

#endif // SAFE_FLASH_BLOCK_DEVICE_H_
