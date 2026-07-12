#ifndef MUSIN_PORTS_PICO_FILESYSTEM_AUDIO_SAFE_FLASH_H_
#define MUSIN_PORTS_PICO_FILESYSTEM_AUDIO_SAFE_FLASH_H_

extern "C" {
#include "blockdevice/blockdevice.h"
}

#include "musin/filesystem/flash_guard.h"
#include <cstdint>

namespace musin::filesystem {

/**
 * @brief The Pico port's FlashGuard: masks all interrupts *below* the audio
 * DMA priority around flash erase/program, so the RAM-resident audio render
 * keeps running while flash is busy.
 *
 * The stock pico-vfs flash blockdevice wraps erase/program in
 * flash_safe_execute, which disables all interrupts for the duration of
 * the operation (tens of milliseconds for a sector erase) and silences
 * audio. This guard keeps the audio interrupt alive instead.
 *
 * Requirements: single-core application, RAM vector table, and an audio
 * interrupt path (handlers, render code and data) that never touches flash.
 */
FlashGuard &audio_priority_flash_guard();

/**
 * @brief Replaces a flash blockdevice's erase/program ops with RAM-resident
 * versions that run the given FlashGuard's enter()/exit() around the raw
 * flash operation.
 *
 * Only one device may be wrapped.
 *
 * @param device The blockdevice returned by blockdevice_flash_create.
 * @param flash_offset The flash offset the device was created with.
 * @param guard The policy to run around each erase/program.
 */
void install_flash_guard(blockdevice_t *device, uint32_t flash_offset,
                         FlashGuard &guard);

} // namespace musin::filesystem

#endif // MUSIN_PORTS_PICO_FILESYSTEM_AUDIO_SAFE_FLASH_H_
