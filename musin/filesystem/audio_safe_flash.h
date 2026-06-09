#ifndef MUSIN_FILESYSTEM_AUDIO_SAFE_FLASH_H_
#define MUSIN_FILESYSTEM_AUDIO_SAFE_FLASH_H_

extern "C" {
#include "blockdevice/blockdevice.h"
}

#include <cstdint>

namespace musin::filesystem {

/**
 * @brief Replaces a flash blockdevice's erase/program ops with versions
 * that keep the audio interrupt running.
 *
 * The stock pico-vfs flash blockdevice wraps erase/program in
 * flash_safe_execute, which disables all interrupts for the duration of
 * the operation (tens of milliseconds for a sector erase) and silences
 * audio. These replacements mask all interrupts *below* the audio DMA
 * priority instead, so the RAM-resident audio render keeps running while
 * flash is busy.
 *
 * Requirements: single-core application, RAM vector table, and an audio
 * interrupt path (handlers, render code and data) that never touches
 * flash. Only one device may be wrapped.
 *
 * @param device The blockdevice returned by blockdevice_flash_create.
 * @param flash_offset The flash offset the device was created with.
 */
void make_flash_blockdevice_audio_safe(blockdevice_t *device,
                                       uint32_t flash_offset);

} // namespace musin::filesystem

#endif // MUSIN_FILESYSTEM_AUDIO_SAFE_FLASH_H_
