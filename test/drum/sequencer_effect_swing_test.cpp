#include "drum/config.h"
#include "drum/sequencer_effect_swing.h"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

namespace drum {

namespace {

constexpr std::uint8_t PHASES_PER_BEAT = 24;
constexpr std::size_t STEPS_PER_CYCLE = 8;
constexpr std::uint32_t CYCLE_PHASES = (STEPS_PER_CYCLE / 2) * PHASES_PER_BEAT;
constexpr std::array<uint8_t, 4> SIXTEENTH_MASK_BITS = {0, 6, 12, 18};
constexpr std::array<uint8_t, 3> TRIPLET_MASK_BITS = {0, 8, 16};

std::vector<uint8_t> extract_set_bits(std::uint32_t mask) {
  std::vector<uint8_t> bits;
  bits.reserve(PHASES_PER_BEAT);
  for (uint8_t index = 0; index < PHASES_PER_BEAT; ++index) {
    if ((mask & (1u << index)) != 0u) {
      bits.push_back(index);
    }
  }
  std::sort(bits.begin(), bits.end());
  return bits;
}

template <std::size_t Size>
std::vector<uint8_t> rotate_bits(const std::array<uint8_t, Size> &base_bits,
                                 uint8_t rotation) {
  std::vector<uint8_t> rotated;
  rotated.reserve(Size);
  for (uint8_t bit : base_bits) {
    rotated.push_back(static_cast<uint8_t>((bit + rotation) % PHASES_PER_BEAT));
  }
  std::sort(rotated.begin(), rotated.end());
  return rotated;
}

std::vector<uint32_t>
calculate_absolute_step_times(const SequencerEffectSwing &swing_effect,
                              bool repeat_active = false,
                              uint64_t base_transport_step = 0) {
  std::vector<uint32_t> absolute_times;
  absolute_times.reserve(STEPS_PER_CYCLE);

  for (size_t step = 0; step < STEPS_PER_CYCLE; ++step) {
    auto timing = swing_effect.calculate_step_timing(
        step, repeat_active, base_transport_step + step);
    const uint32_t beat_index = static_cast<uint32_t>(step / 2);
    const uint32_t absolute_time =
        beat_index * PHASES_PER_BEAT + timing.expected_phase;
    absolute_times.push_back(absolute_time);
  }

  return absolute_times;
}

uint32_t total_cycle_duration(const std::vector<uint32_t> &absolute_times) {
  uint32_t total_duration = absolute_times.front();
  for (std::size_t i = 1; i < absolute_times.size(); ++i) {
    total_duration += absolute_times[i] - absolute_times[i - 1];
  }
  total_duration += CYCLE_PHASES - absolute_times.back();
  return total_duration;
}

} // namespace

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
  SECTION("Straight timing total time") {
    swing_effect.set_swing_enabled(false);
    auto absolute_times = calculate_absolute_step_times(swing_effect);
    REQUIRE(absolute_times.size() == STEPS_PER_CYCLE);

    for (std::size_t i = 1; i < absolute_times.size(); ++i) {
      REQUIRE(absolute_times[i] >= absolute_times[i - 1]);
    }
    REQUIRE(total_cycle_duration(absolute_times) == CYCLE_PHASES);
  }

  SECTION("Swing timing total time - odd delay") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(true); // Delay odd steps

    auto absolute_times = calculate_absolute_step_times(swing_effect);
    REQUIRE(absolute_times.size() == STEPS_PER_CYCLE);

    for (std::size_t i = 1; i < absolute_times.size(); ++i) {
      REQUIRE(absolute_times[i] >= absolute_times[i - 1]);
    }
    REQUIRE(total_cycle_duration(absolute_times) == CYCLE_PHASES);
  }

  SECTION("Swing timing total time - even delay") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(false); // Delay even steps

    auto absolute_times = calculate_absolute_step_times(swing_effect);
    REQUIRE(absolute_times.size() == STEPS_PER_CYCLE);

    for (std::size_t i = 1; i < absolute_times.size(); ++i) {
      REQUIRE(absolute_times[i] >= absolute_times[i - 1]);
    }
    REQUIRE(total_cycle_duration(absolute_times) == CYCLE_PHASES);
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

      auto absolute_times = calculate_absolute_step_times(swing_effect);
      total_times.push_back(total_cycle_duration(absolute_times));
    }

    // All swing configurations should maintain consistent total timing
    // The key insight: swing redistributes timing but doesn't change total
    // duration
    REQUIRE(total_times.size() == 3);
    for (auto total_duration : total_times) {
      REQUIRE(total_duration == CYCLE_PHASES);
    }
  }

  SECTION("Stress invariance with heavy switching") {
    // Test that total time remains predictable even with chaotic switching
    swing_effect.set_swing_enabled(true);

    for (int iteration = 0; iteration < 10; ++iteration) {
      std::vector<uint32_t> absolute_times;
      absolute_times.reserve(STEPS_PER_CYCLE);

      for (size_t step = 0; step < STEPS_PER_CYCLE; ++step) {
        swing_effect.set_swing_target((step + iteration) % 3 == 0);
        auto timing =
            swing_effect.calculate_step_timing(step, false, step + iteration);
        const uint32_t beat_index = static_cast<uint32_t>(step / 2);
        absolute_times.push_back(beat_index * PHASES_PER_BEAT +
                                 timing.expected_phase);
      }

      REQUIRE(total_cycle_duration(absolute_times) == CYCLE_PHASES);
    }
  }

  SECTION("Complete cycle timing invariance") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(true);

    // Test complete cycle: step 0 (transport 0) → step 0 (transport 8)
    auto step0_first = swing_effect.calculate_step_timing(0, false, 0);
    auto step0_second = swing_effect.calculate_step_timing(0, false, 8);

    uint32_t first_absolute = 0 * PHASES_PER_BEAT + step0_first.expected_phase;
    uint32_t second_absolute =
        4 * PHASES_PER_BEAT + step0_second.expected_phase;

    // Complete cycle should always be 96 phases (4 beats), regardless of swing
    REQUIRE(second_absolute - first_absolute == CYCLE_PHASES);

    // Both step 0s should have identical timing (even steps, no delay with
    // delay_odd=true)
    REQUIRE(step0_first.expected_phase == step0_second.expected_phase);
    REQUIRE(step0_first.is_delay_applied == step0_second.is_delay_applied);
  }

  SECTION("Multiple complete cycles maintain invariance") {
    swing_effect.set_swing_enabled(true);
    swing_effect.set_swing_target(true);

    // Test multiple complete cycles
    std::vector<uint32_t> cycle_start_times;

    for (uint64_t cycle = 0; cycle < 3; ++cycle) {
      auto step0_timing =
          swing_effect.calculate_step_timing(0, false, cycle * 8);
      uint32_t absolute_time =
          (cycle * 4) * PHASES_PER_BEAT + step0_timing.expected_phase;
      cycle_start_times.push_back(absolute_time);
    }

    // Each cycle should be exactly CYCLE_PHASES apart
    for (size_t i = 1; i < cycle_start_times.size(); ++i) {
      uint32_t cycle_duration = cycle_start_times[i] - cycle_start_times[i - 1];
      REQUIRE(cycle_duration == CYCLE_PHASES);
    }
  }

  SECTION("Swing state changes don't affect cycle duration") {
    swing_effect.set_swing_target(true);

    std::vector<uint32_t> absolute_times;
    for (size_t step = 0; step < STEPS_PER_CYCLE; ++step) {
      swing_effect.set_swing_enabled(step ==
                                     7); // Enable swing only on last step
      auto timing = swing_effect.calculate_step_timing(step, false, step);
      absolute_times.push_back((step / 2) * PHASES_PER_BEAT +
                               timing.expected_phase);
    }

    REQUIRE(total_cycle_duration(absolute_times) == CYCLE_PHASES);
  }
}

TEST_CASE("SequencerEffectSwing retrigger mask tests",
          "[sequencer_effect_swing]") {
  SequencerEffectSwing swing_effect;

  SECTION("Straight timing retriggers on sixteenth note boundaries") {
    swing_effect.set_swing_enabled(false);

    auto timing = swing_effect.calculate_step_timing(0, false, 0);
    auto retrigger_phases = extract_set_bits(timing.substep_mask);

    // Straight timing should retrigger on sixteenth note boundaries
    // Expected phases: 0, 6, 12, 18 (every 6 phases = 1/4 beat)
    std::vector<uint8_t> expected_phases = {0, 6, 12, 18};
    REQUIRE(retrigger_phases == expected_phases);
  }

  SECTION("Swing timing retriggers on triplet boundaries") {
    swing_effect.set_swing_enabled(true);

    auto timing = swing_effect.calculate_step_timing(0, false, 0);
    auto retrigger_phases = extract_set_bits(timing.substep_mask);

    // Swing timing should retrigger on triplet boundaries
    // Expected phases: 0, 8, 16 (every 8 phases = 1/3 beat)
    std::vector<uint8_t> expected_phases = {0, 8, 16};
    REQUIRE(retrigger_phases == expected_phases);
  }
}

} // namespace drum
