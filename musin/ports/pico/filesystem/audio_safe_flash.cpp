#include "audio_safe_flash.h"

extern "C" {
#include "hardware/flash.h"
#include "pico/multicore.h"
#include "pico/platform/sections.h"
}

#include <cassert>

namespace {

// Flash offset of the wrapped device's first byte. A single data
// partition is assumed; assert in install_flash_guard guards against a
// second.
uint32_t device_flash_offset = 0;
musin::filesystem::FlashGuard *installed_guard = nullptr;

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

// The wrappers bypass flash_safe_execute(), so they offer no core 1
// lockout: they are only safe while core 1 stays parked in the bootrom.
// If core 1 is ever launched, route flash writes through flash_safe_execute
// instead. Must run before BASEPRI masking, while flash (XIP) is readable.
inline void assert_core1_not_running() {
  assert(get_core_num() == 0);
  assert(!multicore_lockout_victim_is_initialized(1));
}

// Guard enter()/exit() run while flash is still readable (before the erase
// starts / after it finishes), so they may live in flash; only the wrapper
// that calls flash_range_erase/program must be RAM-resident.
class AudioPriorityFlashGuard final : public musin::filesystem::FlashGuard {
public:
  void enter() override {
    assert_core1_not_running();
    saved_basepri_ = read_basepri();
    write_basepri(MASK_ALL_BUT_HIGHEST_PRIORITY);
  }

  void exit() override {
    write_basepri(saved_basepri_);
  }

private:
  uint32_t saved_basepri_ = 0;
};

AudioPriorityFlashGuard audio_guard;

int __not_in_flash_func(guarded_erase)(blockdevice_t *device, bd_size_t addr,
                                       bd_size_t size) {
  (void)device;
  installed_guard->enter();
  flash_range_erase(device_flash_offset + (size_t)addr, (size_t)size);
  installed_guard->exit();
  return BD_ERROR_OK;
}

int __not_in_flash_func(guarded_program)(blockdevice_t *device,
                                         const void *buffer, bd_size_t addr,
                                         bd_size_t size) {
  (void)device;
  installed_guard->enter();
  flash_range_program(device_flash_offset + (size_t)addr,
                      (const uint8_t *)buffer, (size_t)size);
  installed_guard->exit();
  return BD_ERROR_OK;
}

} // namespace

namespace musin::filesystem {

FlashGuard &audio_priority_flash_guard() {
  return audio_guard;
}

void install_flash_guard(blockdevice_t *device, uint32_t flash_offset,
                         FlashGuard &guard) {
  assert(installed_guard == nullptr);
  installed_guard = &guard;
  device_flash_offset = flash_offset;
  device->erase = guarded_erase;
  device->program = guarded_program;
}

} // namespace musin::filesystem
