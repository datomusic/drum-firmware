#ifndef MOCK_HARDWARE_INTERP_H_
#define MOCK_HARDWARE_INTERP_H_

#include <cstdint>
#include <numeric>

// This is a mock implementation of the RP2350 hardware interpolator
// for host-based unit testing. It simulates the behavior of linear
// interpolation (blend mode) used by the PitchShifter.

// Mock for interp_config struct
typedef struct {
  bool blend;
  bool is_signed;
} interp_config;

// Global state for testing configuration calls
static interp_config mock_interp0_lane0_cfg;
static interp_config mock_interp0_lane1_cfg;

// Mock for interp_default_config()
static inline interp_config interp_default_config() {
  return interp_config{false, false};
}

// Mock for interp_config_set_blend()
static inline void interp_config_set_blend(interp_config *cfg, bool blend) {
  cfg->blend = blend;
}

// Mock for interp_config_set_signed()
static inline void interp_config_set_signed(interp_config *cfg, bool is_signed) {
  cfg->is_signed = is_signed;
}

// Mock hardware interpolator state
struct interp_hw_t {
  // Mock registers
  volatile int32_t accum[2];
  volatile int32_t base[3];

  // Mock peek registers that perform calculations
  struct PeekProxy {
    const interp_hw_t *parent;

    int32_t operator[](int index) const {
      if (index == 1) { // Blend mode interpolation for PEEK1
        // The fraction is taken from the LSBs of accum[1].
        // The pitch shifter code stores a value from 0-255 here.
        const float fraction = static_cast<float>(parent->accum[1] & 0xFF) / 255.0f;
        // The base registers are written with int16_t, so we should treat them
        // as such. The real hardware would sign-extend them to 32-bit.
        const float y1 = static_cast<float>(static_cast<int16_t>(parent->base[0]));
        const float y2 = static_cast<float>(static_cast<int16_t>(parent->base[1]));

        // Linear interpolation: y1 * (1 - frac) + y2 * frac
        return static_cast<int32_t>(y1 + (y2 - y1) * fraction);
      }
      // Other peek indices are not used in the pitch shifter.
      return 0;
    }
  };

  PeekProxy peek;

  interp_hw_t() : peek{this} {
  }
};

// Global mock instance and pointer, mimicking the SDK's hardware registers
static interp_hw_t mock_interp0_hw;
static interp_hw_t *const interp0 = &mock_interp0_hw;

// Mock for interp_set_config()
static inline void interp_set_config(struct interp_hw_t *hw, uint32_t lane,
                                     const interp_config *cfg) {
  if (hw == &mock_interp0_hw) {
    if (lane == 0) {
      mock_interp0_lane0_cfg = *cfg;
    } else if (lane == 1) {
      mock_interp0_lane1_cfg = *cfg;
    }
  }
}

// Resets the state of the mock interpolator for clean test runs.
static inline void reset_mock_interp_state() {
  mock_interp0_lane0_cfg = {false, false};
  mock_interp0_lane1_cfg = {false, false};
  mock_interp0_hw.accum[0] = 0;
  mock_interp0_hw.accum[1] = 0;
  mock_interp0_hw.base[0] = 0;
  mock_interp0_hw.base[1] = 0;
  mock_interp0_hw.base[2] = 0;
}

#endif // MOCK_HARDWARE_INTERP_H_
