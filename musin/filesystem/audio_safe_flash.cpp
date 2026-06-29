#include "audio_safe_flash.h"

extern "C" {
#include "hardware/flash.h"
#include "pico/multicore.h"
#include "pico/platform/sections.h"
}

#include <cassert>

namespace {

// Flash offset of the wrapped device's first byte. A single data
// partition is assumed; assert in the wrapper guards against a second.
uint32_t device_flash_offset = 0;
bool device_wrapped = false;

// Masks all IRQs with hardware priority >= 0x10, leaving only priority 0x00
// able to preempt (the audio DMA IRQ, set to 0 in AudioOutput::attach_source).
// Any future IRQ whose handler lives in flash MUST be assigned priority >= 0x10
// or it will hard-fault during a flash erase.
constexpr uint32_t MASK_ALL_BUT_HIGHEST_PRIORITY = 0x10;

inline uint32_t read_basepri() {
  uint32_t value;
  __asm volatile("mrs %0, basepri" : "=r"(value));
  return value;
}

inline void write_basepri(uint32_t value) {
  __asm volatile("msr basepri, %0" ::"r"(value) : "memory");
}

// These wrappers bypass flash_safe_execute(), so they offer no core 1
// lockout: they are only safe while core 1 stays parked in the bootrom.
// If core 1 is ever launched, route flash writes through flash_safe_execute
// instead. Must run before BASEPRI masking, while flash (XIP) is readable.
inline void assert_core1_not_running() {
  assert(get_core_num() == 0);
  assert(!multicore_lockout_victim_is_initialized(1));
}

int __not_in_flash_func(erase_keep_audio_running)(blockdevice_t *device,
                                                  bd_size_t addr,
                                                  bd_size_t size) {
  (void)device;
  assert_core1_not_running();
  const uint32_t saved_basepri = read_basepri();
  write_basepri(MASK_ALL_BUT_HIGHEST_PRIORITY);
  flash_range_erase(device_flash_offset + (size_t)addr, (size_t)size);
  write_basepri(saved_basepri);
  return BD_ERROR_OK;
}

int __not_in_flash_func(program_keep_audio_running)(blockdevice_t *device,
                                                    const void *buffer,
                                                    bd_size_t addr,
                                                    bd_size_t size) {
  (void)device;
  assert_core1_not_running();
  const uint32_t saved_basepri = read_basepri();
  write_basepri(MASK_ALL_BUT_HIGHEST_PRIORITY);
  flash_range_program(device_flash_offset + (size_t)addr,
                      (const uint8_t *)buffer, (size_t)size);
  write_basepri(saved_basepri);
  return BD_ERROR_OK;
}

} // namespace

namespace musin::filesystem {

void make_flash_blockdevice_audio_safe(blockdevice_t *device,
                                       uint32_t flash_offset) {
  assert(!device_wrapped);
  device_wrapped = true;
  device_flash_offset = flash_offset;
  device->erase = erase_keep_audio_running;
  device->program = program_keep_audio_running;
}

} // namespace musin::filesystem
