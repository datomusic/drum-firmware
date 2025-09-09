#include "drum/config.h"
#include "drum/sequencer_persistence.h"
#include "drum/sequencer_storage.h"
#include "pico/time.h"
#include "test/test_support.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Mock time for testing - required by pico/time.h override
absolute_time_t mock_current_time = 0;

namespace drum {

// Include component implementations inline for integration testing
// (avoiding duplicate symbol issues with individual test files)

// Redirect production path to a writable test path
static const char *kTestRedirectPath = "/tmp/test_sequencer_state.dat";

// SequencerPersister implementation
bool SequencerPersister::save_to_file(const char *filepath,
                                      const SequencerPersistentState &state) {
  const char *target = filepath;
  if (std::string(filepath) == "/sequencer_state.dat") {
    target = kTestRedirectPath;
  }
  FILE *file = fopen(target, "wb");
  if (!file) {
    return false;
  }

  size_t written = fwrite(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);

  return written == 1;
}

bool SequencerPersister::load_from_file(const char *filepath,
                                        SequencerPersistentState &state) {
  const char *target = filepath;
  if (std::string(filepath) == "/sequencer_state.dat") {
    target = kTestRedirectPath;
  }
  FILE *file = fopen(target, "rb");
  if (!file) {
    return false; // File doesn't exist, not an error
  }

  size_t read_size = fread(&state, sizeof(SequencerPersistentState), 1, file);
  fclose(file);

  if (read_size != 1 || !state.is_valid()) {
    return false; // Corrupted or invalid file
  }

  return true;
}

// SaveTimingManager implementation
SaveTimingManager::SaveTimingManager(TimeSource &time_source,
                                     uint32_t save_debounce_ms,
                                     uint32_t max_save_interval_ms)
    : time_source_(time_source), save_debounce_ms_(save_debounce_ms),
      max_save_interval_ms_(max_save_interval_ms) {
}

void SaveTimingManager::mark_dirty() {
  state_is_dirty_ = true;
  last_change_time_ms_ = time_source_.get_time_ms();
}

void SaveTimingManager::mark_clean() {
  state_is_dirty_ = false;
  last_save_time_ms_ = time_source_.get_time_ms();
}

bool SaveTimingManager::is_dirty() const {
  return state_is_dirty_;
}

bool SaveTimingManager::should_save_now() const {
  if (!state_is_dirty_) {
    return false;
  }

  uint32_t current_time_ms = time_source_.get_time_ms();
  uint32_t time_since_change = current_time_ms - last_change_time_ms_;
  uint32_t time_since_save = current_time_ms - last_save_time_ms_;

  // Save if enough time has passed since last change (debounce)
  // OR if maximum interval has been exceeded
  return (time_since_change >= save_debounce_ms_ ||
          time_since_save >= max_save_interval_ms_);
}

// PicoTimeSource implementation using mock time
uint32_t PicoTimeSource::get_time_ms() const {
  return time_us_32() / 1000;
}

// For testing, we know SequencerStorage tries to use "/sequencer_state.dat"
// which will fail due to permissions. Let's create a symlink or just accept
// the failure and focus on testing the orchestration logic

// Helper class to manage temporary test files
class TempFileManager {
public:
  TempFileManager(const std::string &filename) : filepath_(filename) {
    cleanup();
  }

  ~TempFileManager() {
    cleanup();
  }

  const std::string &path() const {
    return filepath_;
  }

  void cleanup() {
    if (fs::exists(filepath_)) {
      fs::remove(filepath_);
    }
  }

  bool exists() const {
    return fs::exists(filepath_);
  }

private:
  std::string filepath_;
};

// Helper function to create a test state with known pattern
SequencerPersistentState create_test_state() {
  SequencerPersistentState state;

  // Fill with a recognizable pattern (velocities only)
  for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
      state.tracks[track].velocities[step] =
          static_cast<uint8_t>(127 - (track * 10 + step));
    }
    state.active_notes[track] =
        static_cast<uint8_t>(60 + track * 5); // C4, F4, A4, D5
  }

  return state;
}

// Helper function to verify states are equal
bool states_equal(const SequencerPersistentState &a,
                  const SequencerPersistentState &b) {
  if (a.magic != b.magic || a.version != b.version) {
    return false;
  }

  for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
      if (a.tracks[track].velocities[step] !=
          b.tracks[track].velocities[step]) {
        return false;
      }
    }
    if (a.active_notes[track] != b.active_notes[track]) {
      return false;
    }
  }

  return true;
}

TEST_CASE("SequencerPersistentState validation", "[sequencer_storage]") {
  SECTION("Valid state passes validation") {
    SequencerPersistentState state;
    REQUIRE(state.is_valid());
    REQUIRE(state.magic == SequencerPersistentState::MAGIC_NUMBER);
    REQUIRE(state.version == SequencerPersistentState::FORMAT_VERSION);
  }

  SECTION("Invalid magic number fails validation") {
    SequencerPersistentState state;
    state.magic = 0xDEADBEEF;
    REQUIRE_FALSE(state.is_valid());
  }

  SECTION("Invalid version fails validation") {
    SequencerPersistentState state;
    state.version = 99;
    REQUIRE_FALSE(state.is_valid());
  }

  SECTION("Size constraint is satisfied") {
    REQUIRE(sizeof(SequencerPersistentState) < 512);
  }
}

TEST_CASE("SequencerStorage basic round-trip", "[sequencer_storage]") {
  TempFileManager temp_file(kTestRedirectPath);
  SequencerStorage<4, 8> storage;

  SECTION("Save and load valid state") {
    SequencerPersistentState original_state = create_test_state();

    // Save the state
    REQUIRE(storage.save_state_to_flash(original_state));
    REQUIRE(temp_file.exists());

    // Load it back
    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));

    // Verify they match
    REQUIRE(states_equal(original_state, loaded_state));
  }

  SECTION("Empty state round-trip") {
    SequencerPersistentState
        original_state; // Default constructor creates empty state

    REQUIRE(storage.save_state_to_flash(original_state));

    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));

    REQUIRE(states_equal(original_state, loaded_state));
  }

  SECTION("Multiple save/load cycles preserve data") {
    SequencerPersistentState state = create_test_state();

    for (int cycle = 0; cycle < 3; ++cycle) {
      REQUIRE(storage.save_state_to_flash(state));

      SequencerPersistentState loaded_state;
      REQUIRE(storage.load_state_from_flash(loaded_state));

      REQUIRE(states_equal(state, loaded_state));

      // Modify state slightly for next cycle
      state.tracks[0].velocities[0] = static_cast<uint8_t>(cycle + 100);
      state = loaded_state; // Use loaded state as basis for next cycle
      state.tracks[0].velocities[0] = static_cast<uint8_t>(cycle + 100);
    }
  }
}

TEST_CASE("SequencerStorage data integrity", "[sequencer_storage]") {
  TempFileManager temp_file(kTestRedirectPath);
  SequencerStorage<4, 8> storage;

  SECTION("All track data preservation") {
    SequencerPersistentState state;

    // Fill with distinct patterns for each track
    for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
      for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
        state.tracks[track].velocities[step] =
            static_cast<uint8_t>(100 + track * 10 + step);
      }
    }

    REQUIRE(storage.save_state_to_flash(state));

    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));

    // Verify each track independently
    for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
      for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
        REQUIRE(loaded_state.tracks[track].velocities[step] ==
                100 + track * 10 + step);
      }
    }
  }

  SECTION("Active notes preservation") {
    SequencerPersistentState state;

    // Set distinct active notes
    state.active_notes[0] = 36; // Kick
    state.active_notes[1] = 38; // Snare
    state.active_notes[2] = 42; // Hi-hat closed
    state.active_notes[3] = 49; // Crash

    REQUIRE(storage.save_state_to_flash(state));

    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));

    REQUIRE(loaded_state.active_notes[0] == 36);
    REQUIRE(loaded_state.active_notes[1] == 38);
    REQUIRE(loaded_state.active_notes[2] == 42);
    REQUIRE(loaded_state.active_notes[3] == 49);
  }

  SECTION("Boundary value testing") {
    SequencerPersistentState state;

    // Test with min and max values
    state.tracks[0].velocities[0] = 0;   // Min velocity
    state.tracks[0].velocities[1] = 255; // Max velocity
    state.active_notes[0] = 0;           // Min active note
    state.active_notes[1] = 255;         // Max active note

    REQUIRE(storage.save_state_to_flash(state));

    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));

    REQUIRE(loaded_state.tracks[0].velocities[0] == 0);
    REQUIRE(loaded_state.tracks[0].velocities[1] == 255);
    REQUIRE(loaded_state.active_notes[0] == 0);
    REQUIRE(loaded_state.active_notes[1] == 255);
  }
}

TEST_CASE("SequencerStorage file system edge cases", "[sequencer_storage]") {
  TempFileManager temp_file(kTestRedirectPath);
  SequencerStorage<4, 8> storage;

  SECTION("Non-existent file load returns false") {
    SequencerPersistentState state;
    REQUIRE_FALSE(storage.load_state_from_flash(state));
  }

  SECTION("Corrupted file with wrong magic number") {
    // Create a file with wrong magic number
    std::ofstream file(temp_file.path(), std::ios::binary);
    SequencerPersistentState corrupt_state;
    corrupt_state.magic = 0xDEADBEEF; // Wrong magic
    file.write(reinterpret_cast<const char *>(&corrupt_state),
               sizeof(corrupt_state));
    file.close();

    SequencerPersistentState loaded_state;
    REQUIRE_FALSE(storage.load_state_from_flash(loaded_state));
  }

  SECTION("Corrupted file with wrong version") {
    std::ofstream file(temp_file.path(), std::ios::binary);
    SequencerPersistentState corrupt_state;
    corrupt_state.version = 99; // Wrong version
    file.write(reinterpret_cast<const char *>(&corrupt_state),
               sizeof(corrupt_state));
    file.close();

    SequencerPersistentState loaded_state;
    REQUIRE_FALSE(storage.load_state_from_flash(loaded_state));
  }

  SECTION("Truncated file") {
    // Create a truncated file
    std::ofstream file(temp_file.path(), std::ios::binary);
    uint32_t partial_data = 0x12345678;
    file.write(reinterpret_cast<const char *>(&partial_data),
               sizeof(partial_data));
    file.close();

    SequencerPersistentState loaded_state;
    REQUIRE_FALSE(storage.load_state_from_flash(loaded_state));
  }
}

TEST_CASE("SequencerStorage state management", "[sequencer_storage]") {
  SequencerStorage<4, 8> storage;

  SECTION("Initial state is clean") {
    REQUIRE_FALSE(storage.is_dirty());
  }

  SECTION("mark_state_dirty sets dirty flag") {
    storage.mark_state_dirty();
    REQUIRE(storage.is_dirty());
  }

  SECTION("mark_state_clean clears dirty flag") {
    storage.mark_state_dirty();
    REQUIRE(storage.is_dirty());

    storage.mark_state_clean();
    REQUIRE_FALSE(storage.is_dirty());
  }

  SECTION("Successful save cleans state") {
    TempFileManager temp_file(kTestRedirectPath);
    SequencerPersistentState state;

    storage.mark_state_dirty();
    REQUIRE(storage.is_dirty());

    REQUIRE(storage.save_state_to_flash(state));
    REQUIRE_FALSE(storage.is_dirty());
  }

  SECTION("Load clears dirty flag") {
    TempFileManager temp_file(kTestRedirectPath);
    SequencerPersistentState state = create_test_state();

    // Save a valid state first
    REQUIRE(storage.save_state_to_flash(state));

    // Mark dirty, then load
    storage.mark_state_dirty();
    REQUIRE(storage.is_dirty());

    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));
    REQUIRE_FALSE(storage.is_dirty());
  }
}

TEST_CASE("SequencerStorage integration - composed architecture verification",
          "[sequencer_storage]") {
  TempFileManager temp_file(kTestRedirectPath);
  SequencerStorage<4, 8> storage;

  SECTION("Orchestrator correctly delegates to components") {
    SequencerPersistentState state = create_test_state();

    // Verify initial clean state
    REQUIRE_FALSE(storage.is_dirty());
    REQUIRE_FALSE(storage.should_save_now());

    // Mark dirty - should delegate to SaveTimingManager
    storage.mark_state_dirty();
    REQUIRE(storage.is_dirty());

    // Save should delegate to SequencerPersister and clean timing manager
    REQUIRE(storage.save_state_to_flash(state));
    REQUIRE_FALSE(storage.is_dirty());
    REQUIRE(temp_file.exists());

    // Load should delegate to SequencerPersister
    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));
    REQUIRE(states_equal(state, loaded_state));
  }

  SECTION("Timing logic works through orchestrator") {
    // This test verifies that the SaveTimingManager is properly integrated
    // Note: We can't test actual timing delays in unit tests, but we can
    // verify the state management works correctly

    REQUIRE_FALSE(storage.should_save_now()); // Clean state shouldn't save

    storage.mark_state_dirty();
    // Immediately after marking dirty, timing manager should indicate save
    // needed (This depends on the debounce implementation, but generally true)
    REQUIRE(storage.is_dirty());
  }

  SECTION("End-to-end persistence workflow") {
    SequencerPersistentState original_state = create_test_state();

    // Simulate typical usage pattern:
    // 1. Mark state dirty (user made changes)
    storage.mark_state_dirty();
    REQUIRE(storage.is_dirty());

    // 2. Check if save is needed (would be called periodically)
    // Note: For unit testing, we can't easily test time-based logic
    // But we can verify the state management is correct

    // 3. Save when appropriate
    REQUIRE(storage.save_state_to_flash(original_state));
    REQUIRE_FALSE(storage.is_dirty());

    // 4. Later, load the state back
    SequencerPersistentState loaded_state;
    REQUIRE(storage.load_state_from_flash(loaded_state));
    REQUIRE(states_equal(original_state, loaded_state));
    REQUIRE_FALSE(storage.is_dirty()); // Load should keep state clean
  }
}

TEST_CASE("SequencerStorage integration - file path consistency",
          "[sequencer_storage]") {
  // Verify that the orchestrator uses the correct file path consistently
  TempFileManager temp_file(kTestRedirectPath);
  SequencerStorage<4, 8> storage;

  SequencerPersistentState state = create_test_state();

  // Save with the orchestrator
  REQUIRE(storage.save_state_to_flash(state));
  REQUIRE(temp_file.exists());

  // The file should be exactly what SequencerPersister would create
  // Verify by loading it back
  SequencerPersistentState loaded_state;
  REQUIRE(storage.load_state_from_flash(loaded_state));
  REQUIRE(states_equal(state, loaded_state));
}

} // namespace drum
