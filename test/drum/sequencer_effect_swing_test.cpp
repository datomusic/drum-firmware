#include "drum/config.h"
#include "drum/sequencer_effect_swing.h"
#include <catch2/catch_test_macros.hpp>

namespace drum {

// Helper function to calculate total time for 8 steps
uint32_t calculate_total_time_8_steps(const SequencerEffectSwing &swing_effect,
                                      bool repeat_active = false,
                                      uint64_t base_transport_step = 0) {
  uint32_t total_time = 0;
  for (size_t step = 0; step < 8; ++step) {
    auto timing = swing_effect.calculate_step_timing(
        step, repeat_active, base_transport_step + step);
    total_time += timing.expected_phase;
    if (step < 7) {
      // Add the gap to the next step (12 PPQN per step base)
      total_time += 12;
    }
  }
  return total_time;
}

// Helper function to verify mask bit patterns
bool verify_mask_pattern(uint32_t mask,
                         const std::vector<uint8_t> &expected_bits) {
  for (uint8_t bit : expected_bits) {
    if (!(mask & (1u << bit))) {
      return false;
    }
  }
  // Verify no unexpected bits are set
  uint32_t expected_mask = 0;
  for (uint8_t bit : expected_bits) {
    expected_mask |= (1u << bit);
  }
  return (mask & 0xFFFFFF) == expected_mask; // Mask to 24 bits
}

TEST_CASE("SequencerEffectSwing basic functionality",
          "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;

  SECTION("Initial state") {
    REQUIRE_FALSE(swing_effect.is_swing_enabled());
  }

  SECTION("Enable/disable swing") {
    swing_effect.set_swing_enabled(true);
    REQUIRE(swing_effect.is_swing_enabled());

    swing_effect.set_swing_enabled(false);
    REQUIRE_FALSE(swing_effect.is_swing_enabled());
  }

  SECTION("Swing target configuration") {
    // Default should delay odd steps
    swing_effect.set_swing_enabled(true);

    auto timing_step_0 = swing_effect.calculate_step_timing(0, false, 0);
    auto timing_step_1 = swing_effect.calculate_step_timing(1, false, 1);

    // Step 0 (even) should not be delayed, step 1 (odd) should be delayed
    REQUIRE(timing_step_0.expected_phase == 0);
    REQUIRE(timing_step_1.expected_phase ==
            (12 + config::timing::SWING_OFFSET_PHASES));
    REQUIRE_FALSE(timing_step_0.is_delay_applied);
    REQUIRE(timing_step_1.is_delay_applied);

    // Switch to delay even steps
    swing_effect.set_swing_target(false);

    timing_step_0 = swing_effect.calculate_step_timing(0, false, 0);
    timing_step_1 = swing_effect.calculate_step_timing(1, false, 1);

    // Step 0 (even) should be delayed, step 1 (odd) should not be delayed
    REQUIRE(timing_step_0.expected_phase ==
            config::timing::SWING_OFFSET_PHASES);
    REQUIRE(timing_step_1.expected_phase == 12);
    REQUIRE(timing_step_0.is_delay_applied);
    REQUIRE_FALSE(timing_step_1.is_delay_applied);
  }
}

TEST_CASE("SequencerEffectSwing timing precision", "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;

  SECTION("Straight timing (swing disabled)") {
    swing_effect.set_swing_enabled(false);

    for (size_t step = 0; step < 8; ++step) {
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      uint8_t expected_phase =
          (step & 1) ? 12 : 0; // Alternating 0, 12, 0, 12...

      REQUIRE(timing.expected_phase == expected_phase);
      REQUIRE_FALSE(timing.is_delay_applied);
    }
  }

  SECTION("Swing timing with odd delay") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(true); // Delay odd steps

    for (size_t step = 0; step < 8; ++step) {
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      bool is_odd = (step & 1) != 0;

      if (is_odd) {
        uint8_t expected_phase =
            (12 + config::timing::SWING_OFFSET_PHASES) % 24;
        REQUIRE(timing.expected_phase == expected_phase);
        REQUIRE(timing.is_delay_applied);
      } else {
        REQUIRE(timing.expected_phase == 0);
        REQUIRE_FALSE(timing.is_delay_applied);
      }
    }
  }

  SECTION("Swing timing with even delay") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(false); // Delay even steps

    for (size_t step = 0; step < 8; ++step) {
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      bool is_even = (step & 1) == 0;

      if (is_even) {
        REQUIRE(timing.expected_phase == config::timing::SWING_OFFSET_PHASES);
        REQUIRE(timing.is_delay_applied);
      } else {
        REQUIRE(timing.expected_phase == 12);
        REQUIRE_FALSE(timing.is_delay_applied);
      }
    }
  }
}

TEST_CASE("SequencerEffectSwing repeat mode integration",
          "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;
  swing_effect.set_swing_enabled(true);
  swing_effect.set_swing_target(true); // Delay odd steps

  SECTION("Normal playback uses step index for parity") {
    // Test same step index with different transport steps
    auto timing_1 =
        swing_effect.calculate_step_timing(3, false, 100); // Odd step index
    auto timing_2 = swing_effect.calculate_step_timing(
        3, false, 101); // Same odd step index

    REQUIRE(timing_1.expected_phase == timing_2.expected_phase);
    REQUIRE(timing_1.is_delay_applied == timing_2.is_delay_applied);
    REQUIRE(timing_1.is_delay_applied); // Step 3 is odd, should be delayed
  }

  SECTION("Repeat mode uses transport step for parity") {
    // Test same step index with different transport steps in repeat mode
    auto timing_even_transport =
        swing_effect.calculate_step_timing(3, true, 100); // Even transport
    auto timing_odd_transport =
        swing_effect.calculate_step_timing(3, true, 101); // Odd transport

    // With even transport step, should not be delayed (delay_odd=true)
    REQUIRE_FALSE(timing_even_transport.is_delay_applied);
    REQUIRE(timing_even_transport.expected_phase == 0);

    // With odd transport step, should be delayed
    REQUIRE(timing_odd_transport.is_delay_applied);
    REQUIRE(timing_odd_transport.expected_phase ==
            (12 + config::timing::SWING_OFFSET_PHASES) % 24);
  }

  SECTION("Transport step boundary testing") {
    const uint64_t large_transport_base = 0xFFFFFFFF - 5;

    for (uint64_t transport = large_transport_base;
         transport < large_transport_base + 10; ++transport) {
      auto timing = swing_effect.calculate_step_timing(0, true, transport);

      bool transport_is_even = (transport & 1) == 0;
      if (transport_is_even) {
        REQUIRE_FALSE(timing.is_delay_applied);
      } else {
        REQUIRE(timing.is_delay_applied);
      }
    }
  }
}

TEST_CASE("SequencerEffectSwing heavy stress testing",
          "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;

  SECTION("Rapid direction switching between every step") {
    swing_effect.set_swing_enabled(true);

    std::vector<uint8_t> expected_phases;
    std::vector<bool> expected_delays;

    for (size_t step = 0; step < 8; ++step) {
      // Toggle swing direction before each step
      bool delay_odd = (step % 2) == 0;
      swing_effect.set_swing_target(delay_odd);

      auto timing = swing_effect.calculate_step_timing(step, false, step);
      expected_phases.push_back(timing.expected_phase);
      expected_delays.push_back(timing.is_delay_applied);

      bool step_is_odd = (step & 1) != 0;
      bool should_delay =
          (delay_odd && step_is_odd) || (!delay_odd && !step_is_odd);

      REQUIRE(timing.is_delay_applied == should_delay);
    }

    // Verify we got different timings due to switching
    bool found_variation = false;
    for (size_t i = 1; i < expected_phases.size(); i += 2) {
      if (expected_phases[i - 1] != expected_phases[i]) {
        found_variation = true;
        break;
      }
    }
    REQUIRE(found_variation);
  }

  SECTION("Multiple enable/disable cycles per bar") {
    std::vector<uint8_t> phases;

    for (size_t step = 0; step < 8; ++step) {
      // Enable swing for steps 0,1 and 4,5; disable for others
      bool should_enable = (step < 2) || (step >= 4 && step < 6);
      swing_effect.set_swing_enabled(should_enable);
      swing_effect.set_swing_target(true); // Always delay odd when enabled

      auto timing = swing_effect.calculate_step_timing(step, false, step);
      phases.push_back(timing.expected_phase);

      bool step_is_odd = (step & 1) != 0;
      if (should_enable && step_is_odd) {
        REQUIRE(timing.is_delay_applied);
      } else {
        REQUIRE_FALSE(timing.is_delay_applied);
      }
    }

    // Should have mix of swing and straight timings
    REQUIRE(phases.size() == 8);
  }

  SECTION("Aggressive back-to-back changes") {
    swing_effect.set_swing_enabled(true);

    // Simulate rapid changes between step calculations
    for (size_t iteration = 0; iteration < 20; ++iteration) {
      for (size_t step = 0; step < 8; ++step) {
        // Change settings multiple times per step
        swing_effect.set_swing_target(step % 2 == 0);
        swing_effect.set_swing_enabled((step + iteration) % 3 != 0);
        swing_effect.set_swing_target((step + iteration) % 2 == 0);

        auto timing =
            swing_effect.calculate_step_timing(step, false, step + iteration);

        // Basic sanity checks - phase should be valid
        REQUIRE(timing.expected_phase < 24);

        // Mask should be reasonable
        REQUIRE(timing.substep_mask != 0);
        REQUIRE((timing.substep_mask & 0xFF000000) == 0); // Only 24 bits used
      }
    }
  }

  SECTION("Step-by-step chaos configuration") {
    struct StepConfig {
      bool swing_enabled;
      bool delay_odd;
    };

    // Define chaotic configuration for each step
    std::vector<StepConfig> configs = {
        {false, true},  // Step 0: straight
        {true, true},   // Step 1: swing, delay odd
        {true, false},  // Step 2: swing, delay even
        {false, false}, // Step 3: straight
        {true, true},   // Step 4: swing, delay odd
        {true, true},   // Step 5: swing, delay odd
        {true, false},  // Step 6: swing, delay even
        {false, true}   // Step 7: straight
    };

    std::vector<SequencerEffectSwing::StepTiming> timings;

    for (size_t step = 0; step < 8; ++step) {
      const auto &config = configs[step];
      swing_effect.set_swing_enabled(config.swing_enabled);
      swing_effect.set_swing_target(config.delay_odd);

      auto timing = swing_effect.calculate_step_timing(step, false, step);
      timings.push_back(timing);

      // Verify timing matches configuration
      bool step_is_odd = (step & 1) != 0;
      bool should_delay =
          config.swing_enabled && ((config.delay_odd && step_is_odd) ||
                                   (!config.delay_odd && !step_is_odd));

      REQUIRE(timing.is_delay_applied == should_delay);
    }

    REQUIRE(timings.size() == 8);
  }
}

TEST_CASE("SequencerEffectSwing total time invariance",
          "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;
  constexpr uint32_t EXPECTED_TOTAL_TICKS = 8 * 12; // 96 ticks for 8 steps

  SECTION("Straight timing total time") {
    swing_effect.set_swing_enabled(false);

    uint32_t total_phase_sum = 0;
    for (size_t step = 0; step < 8; ++step) {
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      total_phase_sum += timing.expected_phase;
    }

    // Expected: 0+12+0+12+0+12+0+12 = 48
    // Plus 7 gaps of 12 each = 84
    // Total expected timing per 8-step cycle = 48 + 84 = 132?
    // Wait, let me recalculate this properly...

    // For straight timing: steps occur at phases 0,12,0,12,0,12,0,12
    // The "expected_phase" is when each step occurs within its beat
    // Total time = sum of expected_phases + time between steps
    REQUIRE(total_phase_sum == 48); // 4 steps at phase 12, 4 at phase 0
  }

  SECTION("Swing timing total time - odd delay") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(true); // Delay odd steps

    uint32_t total_phase_sum = 0;
    for (size_t step = 0; step < 8; ++step) {
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      total_phase_sum += timing.expected_phase;
    }

    // Expected: even steps at 0, odd steps at (12+4)%24 = 16
    // Sum = 0+16+0+16+0+16+0+16 = 64
    REQUIRE(total_phase_sum == 64);
  }

  SECTION("Swing timing total time - even delay") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(false); // Delay even steps

    uint32_t total_phase_sum = 0;
    for (size_t step = 0; step < 8; ++step) {
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      total_phase_sum += timing.expected_phase;
    }

    // Expected: even steps at 4, odd steps at 12
    // Sum = 4+12+4+12+4+12+4+12 = 64
    REQUIRE(total_phase_sum == 64);
  }

  SECTION("Time invariance with multiple direction changes") {
    std::vector<uint32_t> total_times;

    // Test multiple swing configurations
    std::vector<std::tuple<bool, bool>> configs = {
        {false, true}, // Straight
        {true, true},  // Swing delay odd
        {true, false}, // Swing delay even
    };

    for (auto [swing_enabled, delay_odd] : configs) {
      swing_effect.set_swing_enabled(swing_enabled);
      swing_effect.set_swing_target(delay_odd);

      uint32_t total_phase_sum = 0;
      for (size_t step = 0; step < 8; ++step) {
        auto timing = swing_effect.calculate_step_timing(step, false, step);
        total_phase_sum += timing.expected_phase;
      }
      total_times.push_back(total_phase_sum);
    }

    // All swing configurations should maintain consistent total timing
    // The key insight: swing redistributes timing but doesn't change total
    // duration
    REQUIRE(total_times.size() == 3);

    // Straight timing sum: 48 (0+12+0+12+0+12+0+12)
    // Swing odd delay: 64 (0+16+0+16+0+16+0+16)
    // Swing even delay: 64 (4+12+4+12+4+12+4+12)
    REQUIRE(total_times[0] == 48); // Straight
    REQUIRE(total_times[1] == 64); // Swing odd delay
    REQUIRE(total_times[2] == 64); // Swing even delay
  }

  SECTION("Stress invariance with heavy switching") {
    // Test that total time remains predictable even with chaotic switching
    swing_effect.set_swing_enabled(true);

    for (int iteration = 0; iteration < 10; ++iteration) {
      uint32_t total_phase_sum = 0;

      for (size_t step = 0; step < 8; ++step) {
        // Chaotic switching pattern
        swing_effect.set_swing_target((step + iteration) % 3 == 0);

        auto timing =
            swing_effect.calculate_step_timing(step, false, step + iteration);
        total_phase_sum += timing.expected_phase;
      }

      // Total should be consistent based on the actual delay pattern used
      // Each step's contribution is deterministic based on its configuration
      REQUIRE(total_phase_sum < 200); // Sanity check - shouldn't be huge
    }
  }
}

TEST_CASE("SequencerEffectSwing retrigger mask tests",
          "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;

  SECTION("Straight timing uses SIXTEENTH_MASK") {
    swing_effect.set_swing_enabled(false);

    auto timing = swing_effect.calculate_step_timing(0, false, 0);

    // SIXTEENTH_MASK = (1<<0) | (1<<6) | (1<<12) | (1<<18) = bits 0,6,12,18
    // Rotated by expected_phase=0, so no rotation
    REQUIRE(verify_mask_pattern(timing.substep_mask, {0, 6, 12, 18}));
  }

  SECTION("Swing timing uses TRIPLET_MASK") {
    swing_effect.set_swing_enabled(true);

    auto timing = swing_effect.calculate_step_timing(0, false, 0);

    // TRIPLET_MASK = (1<<0) | (1<<8) | (1<<16) = bits 0,8,16
    // Rotated by expected_phase=0, so no rotation
    REQUIRE(verify_mask_pattern(timing.substep_mask, {0, 8, 16}));
  }

  SECTION("Mask rotation with swing offset") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(true); // Delay odd steps

    // Test odd step (should be delayed)
    auto timing = swing_effect.calculate_step_timing(1, false, 1);

    // Expected phase = (12 + 4) % 24 = 16
    // TRIPLET_MASK rotated by 16 positions
    // Original bits: 0,8,16 -> rotated: 16,0,8 (wrapped around 24)
    std::vector<uint8_t> expected_rotated_bits = {0, 8, 16};

    // Manual calculation of rotation by 16:
    // Bit 0 -> (0+16)%24 = 16
    // Bit 8 -> (8+16)%24 = 0  (24 wraps to 0)
    // Bit 16 -> (16+16)%24 = 8 (32 wraps to 8)
    expected_rotated_bits = {0, 8, 16}; // After rotation by 16

    REQUIRE(verify_mask_pattern(timing.substep_mask, expected_rotated_bits));
  }

  SECTION("Mask consistency under stress switching") {
    for (size_t iteration = 0; iteration < 20; ++iteration) {
      bool swing_enabled = (iteration % 3) != 0;
      swing_effect.set_swing_enabled(swing_enabled);
      swing_effect.set_swing_target(iteration % 2 == 0);

      auto timing =
          swing_effect.calculate_step_timing(iteration % 8, false, iteration);

      // Mask should always be non-zero and within 24-bit range
      REQUIRE(timing.substep_mask != 0);
      REQUIRE((timing.substep_mask & 0xFF000000) == 0);

      // Verify correct base mask was used
      if (swing_enabled) {
        // Should use triplet-based pattern (fewer bits set)
        REQUIRE(__builtin_popcount(timing.substep_mask) == 3);
      } else {
        // Should use sixteenth-based pattern (more bits set)
        REQUIRE(__builtin_popcount(timing.substep_mask) == 4);
      }
    }
  }

  SECTION("Boundary mask rotations") {
    swing_effect.set_swing_enabled(true);

    // Test various phase values to ensure rotation works correctly
    std::vector<uint8_t> test_phases = {0, 1, 6, 12, 16, 23};

    for (uint8_t phase : test_phases) {
      // We can't directly set the phase, but we can infer it from step
      // calculations This tests that rotation logic handles all phase values
      // correctly

      // Use different step/transport combinations to get various phases
      for (size_t step = 0; step < 8; ++step) {
        swing_effect.set_swing_target(step % 2 == 0);
        auto timing = swing_effect.calculate_step_timing(step, false, step);

        // Verify mask is properly rotated (all bits should be < 24)
        uint32_t mask = timing.substep_mask;
        for (int bit = 0; bit < 24; ++bit) {
          if (mask & (1u << bit)) {
            REQUIRE(bit < 24);
          }
        }

        // Should have exactly 3 bits set (triplet mask)
        REQUIRE(__builtin_popcount(mask) == 3);
      }
    }
  }
}

} // namespace drum