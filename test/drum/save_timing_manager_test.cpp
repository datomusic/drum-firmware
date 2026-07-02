#include "drum/save_timing_manager.h"

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include "pico/time.h"
}

// Backing storage for the mock pico/time.h override, referenced by
// PicoTimeSource in save_timing_manager.cpp.
absolute_time_t mock_current_time = 0;

namespace drum {

/**
 * @brief Mock time source for testing SaveTimingManager.
 */
class MockTimeSource : public TimeSource {
public:
  MockTimeSource(uint32_t initial_time_ms = 0)
      : current_time_ms_(initial_time_ms) {
  }

  uint32_t get_time_ms() const override {
    return current_time_ms_;
  }

  void advance_time(uint32_t ms) {
    current_time_ms_ += ms;
  }

  void set_time(uint32_t ms) {
    current_time_ms_ = ms;
  }

private:
  uint32_t current_time_ms_;
};

TEST_CASE("SaveTimingManager basic state management",
          "[save_timing_manager]") {
  MockTimeSource mock_time(1000); // Start at 1000ms
  SaveTimingManager timing(mock_time);

  SECTION("Initial state is clean") {
    REQUIRE_FALSE(timing.is_dirty());
    REQUIRE_FALSE(timing.should_save_now());
  }

  SECTION("mark_dirty sets dirty flag") {
    timing.mark_dirty();
    REQUIRE(timing.is_dirty());
  }

  SECTION("mark_clean clears dirty flag") {
    timing.mark_dirty();
    REQUIRE(timing.is_dirty());

    timing.mark_clean();
    REQUIRE_FALSE(timing.is_dirty());
  }
}

TEST_CASE("SaveTimingManager debounce logic", "[save_timing_manager]") {
  MockTimeSource mock_time(1000); // Start at 1000ms
  uint32_t debounce_ms = 2000;
  uint32_t max_interval_ms = 30000;
  SaveTimingManager timing(mock_time, debounce_ms, max_interval_ms);

  SECTION("should_save_now respects debounce period") {
    // Mark dirty at time 1000
    timing.mark_dirty();
    REQUIRE(timing.is_dirty());
    REQUIRE_FALSE(timing.should_save_now()); // Too soon

    // Advance time but still within debounce period
    mock_time.advance_time(1500); // Now at 2500ms, only 1500ms since dirty
    REQUIRE_FALSE(timing.should_save_now()); // Still too soon

    // Advance past debounce period
    mock_time.advance_time(600); // Now at 3100ms, 2100ms since dirty
    REQUIRE(timing.should_save_now()); // Should save now
  }

  SECTION("should_save_now enforces maximum save interval") {
    // Mark dirty at time 1000
    timing.mark_dirty();

    // Advance to just before max interval
    mock_time.advance_time(max_interval_ms -
                           100); // 29900ms since last save (at time 0)
    REQUIRE(timing.should_save_now()); // Should save due to max interval
  }

  SECTION("Clean state does not trigger saves") {
    // Don't mark dirty
    REQUIRE_FALSE(timing.is_dirty());

    // Advance time significantly
    mock_time.advance_time(max_interval_ms * 2);
    REQUIRE_FALSE(timing.should_save_now()); // Still shouldn't save
  }
}

TEST_CASE("SaveTimingManager timing scenarios", "[save_timing_manager]") {
  MockTimeSource mock_time(0);
  SaveTimingManager timing(mock_time, 1000, 10000); // 1s debounce, 10s max

  SECTION("Multiple mark_dirty calls update timestamp") {
    // First mark dirty
    mock_time.set_time(1000);
    timing.mark_dirty();

    // Advance time partially
    mock_time.set_time(1800);                // 800ms later
    REQUIRE_FALSE(timing.should_save_now()); // Not past debounce yet

    // Mark dirty again - should reset debounce timer
    timing.mark_dirty();

    // Advance another 800ms (1600ms from original, 800ms from second mark)
    mock_time.set_time(2600);
    REQUIRE_FALSE(timing.should_save_now()); // Debounce reset

    // Advance to complete debounce from second mark
    mock_time.set_time(2801); // 1001ms from second mark
    REQUIRE(timing.should_save_now());
  }

  SECTION("mark_clean resets save timestamp") {
    // Mark dirty and save
    mock_time.set_time(1000);
    timing.mark_dirty();

    mock_time.set_time(2100); // Past debounce
    REQUIRE(timing.should_save_now());

    // Clean the state (simulate successful save)
    timing.mark_clean();
    REQUIRE_FALSE(timing.is_dirty());

    // Mark dirty again soon after cleaning
    mock_time.set_time(2200);
    timing.mark_dirty();

    // Should need full debounce period from new dirty time
    mock_time.set_time(3100); // 900ms since new dirty time
    REQUIRE_FALSE(timing.should_save_now());

    mock_time.set_time(3201); // 1001ms since new dirty time
    REQUIRE(timing.should_save_now());
  }
}

TEST_CASE("SaveTimingManager custom timing parameters",
          "[save_timing_manager]") {
  MockTimeSource mock_time(0);

  SECTION("Custom debounce period") {
    SaveTimingManager timing(mock_time, 5000, 30000); // 5s debounce

    mock_time.set_time(1000);
    timing.mark_dirty();

    mock_time.set_time(5999); // Just under 5s debounce
    REQUIRE_FALSE(timing.should_save_now());

    mock_time.set_time(6001); // Just over 5s debounce
    REQUIRE(timing.should_save_now());
  }

  SECTION("Custom max interval") {
    SaveTimingManager timing(mock_time, 1000, 15000); // 15s max interval

    mock_time.set_time(1000);
    timing.mark_dirty();

    // Don't wait for debounce, jump to max interval
    mock_time.set_time(15001); // 15001ms since start (last save time = 0)
    REQUIRE(timing.should_save_now());
  }
}

} // namespace drum
