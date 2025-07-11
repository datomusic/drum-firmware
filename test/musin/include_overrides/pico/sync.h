#ifndef MOCK_PICO_SYNC_H
#define MOCK_PICO_SYNC_H

#include <cstdint>

// Mock spin_lock_t as a simple integer type
typedef uint32_t spin_lock_t;

// Mock spin_lock_init to return a dummy value
static inline spin_lock_t* spin_lock_init(uint32_t lock_num) {
    static spin_lock_t dummy_lock;
    return &dummy_lock;
}

// Mock spin_lock_claim_unused to return a dummy value
static inline uint32_t spin_lock_claim_unused(bool required) {
    return 1; // Return a dummy lock number
}

// Mock spin_lock_blocking to return a dummy value
static inline uint32_t spin_lock_blocking(spin_lock_t* lock) {
    return 0; // Return a dummy IRQ status
}

// Mock spin_unlock to be a no-op
static inline void spin_unlock(spin_lock_t* lock, uint32_t saved_irq) {
    // No-op
}

#endif // MOCK_PICO_SYNC_H
