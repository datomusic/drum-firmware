#ifndef MOCK_HARDWARE_SYNC_H
#define MOCK_HARDWARE_SYNC_H

#include <cstdint>

// Host-test mock: interrupt masking is a no-op off-target.
static inline uint32_t save_and_disable_interrupts(void) {
  return 0;
}

static inline void restore_interrupts(uint32_t saved) {
  (void)saved;
}

#endif // MOCK_HARDWARE_SYNC_H
