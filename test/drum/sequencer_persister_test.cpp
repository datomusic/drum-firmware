#include "drum/sequencer_persister.h"
#include "drum/sequencer_persistence.h"
#include "drum/config.h"
#include "test/test_support.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace drum {

// Helper class to manage temporary test files
class TempFileManager {
public:
  TempFileManager(const std::string& filename) : filepath_(filename) {
    cleanup();
  }
  
  ~TempFileManager() {
    cleanup();
  }
  
  const std::string& path() const { return filepath_; }
  
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
      state.tracks[track].velocities[step] = static_cast<uint8_t>(127 - (track * 10 + step));
    }
    state.active_notes[track] = static_cast<uint8_t>(60 + track * 5); // C4, F4, A4, D5
  }
  
  return state;
}

// Helper function to verify states are equal
bool states_equal(const SequencerPersistentState& a, const SequencerPersistentState& b) {
  if (a.magic != b.magic || a.version != b.version) {
    return false;
  }
  
  for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
    for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
      if (a.tracks[track].velocities[step] != b.tracks[track].velocities[step]) {
        return false;
      }
    }
    if (a.active_notes[track] != b.active_notes[track]) {
      return false;
    }
  }
  
  return true;
}

TEST_CASE("SequencerPersister basic round-trip", "[sequencer_persister]") {
  TempFileManager temp_file("/tmp/test_persister.dat");
  SequencerPersister persister;
  
  SECTION("Save and load valid state") {
    SequencerPersistentState original_state = create_test_state();
    
    // Save the state
    REQUIRE(persister.save_to_file(temp_file.path().c_str(), original_state));
    REQUIRE(temp_file.exists());
    
    // Load it back
    SequencerPersistentState loaded_state;
    REQUIRE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
    
    // Verify they match
    REQUIRE(states_equal(original_state, loaded_state));
  }
  
  SECTION("Empty state round-trip") {
    SequencerPersistentState original_state; // Default constructor creates empty state
    
    REQUIRE(persister.save_to_file(temp_file.path().c_str(), original_state));
    
    SequencerPersistentState loaded_state;
    REQUIRE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
    
    REQUIRE(states_equal(original_state, loaded_state));
  }
  
  SECTION("Multiple save/load cycles preserve data") {
    SequencerPersistentState state = create_test_state();
    
    for (int cycle = 0; cycle < 3; ++cycle) {
      REQUIRE(persister.save_to_file(temp_file.path().c_str(), state));
      
      SequencerPersistentState loaded_state;
      REQUIRE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
      
      REQUIRE(states_equal(state, loaded_state));
      
      // Modify state slightly for next cycle (velocity tweak)
      state.tracks[0].velocities[0] = static_cast<uint8_t>(cycle + 100);
      state = loaded_state; // Use loaded state as basis for next cycle
      state.tracks[0].velocities[0] = static_cast<uint8_t>(cycle + 100);
    }
  }
}

TEST_CASE("SequencerPersister data integrity", "[sequencer_persister]") {
  TempFileManager temp_file("/tmp/test_persister_integrity.dat");
  SequencerPersister persister;
  
  SECTION("All track data preservation") {
    SequencerPersistentState state;
    
    // Fill with distinct patterns for each track (velocities only)
    for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
      for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
        state.tracks[track].velocities[step] = static_cast<uint8_t>(100 + track * 10 + step);
      }
    }
    
    REQUIRE(persister.save_to_file(temp_file.path().c_str(), state));
    
    SequencerPersistentState loaded_state;
    REQUIRE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
    
    // Verify each track independently
    for (size_t track = 0; track < config::NUM_TRACKS; ++track) {
      for (size_t step = 0; step < config::NUM_STEPS_PER_TRACK; ++step) {
        REQUIRE(loaded_state.tracks[track].velocities[step] ==
                100 + track * 10 + step);
      }
    }
  }
  
  SECTION("Boundary value testing") {
    SequencerPersistentState state;
    
    // Test with min and max values
    state.tracks[0].velocities[0] = 0;     // Min velocity
    state.tracks[0].velocities[1] = 255;   // Max velocity
    state.active_notes[0] = 0;        // Min active note
    state.active_notes[1] = 255;      // Max active note
    
    REQUIRE(persister.save_to_file(temp_file.path().c_str(), state));
    
    SequencerPersistentState loaded_state;
    REQUIRE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
    
    REQUIRE(loaded_state.tracks[0].velocities[0] == 0);
    REQUIRE(loaded_state.tracks[0].velocities[1] == 255);
    REQUIRE(loaded_state.active_notes[0] == 0);
    REQUIRE(loaded_state.active_notes[1] == 255);
  }
}

TEST_CASE("SequencerPersister file system edge cases", "[sequencer_persister]") {
  TempFileManager temp_file("/tmp/test_persister_edge_cases.dat");
  SequencerPersister persister;
  
  SECTION("Non-existent file load returns false") {
    SequencerPersistentState state;
    REQUIRE_FALSE(persister.load_from_file("/tmp/nonexistent_file.dat", state));
  }
  
  SECTION("Corrupted file with wrong magic number") {
    // Create a file with wrong magic number
    std::ofstream file(temp_file.path(), std::ios::binary);
    SequencerPersistentState corrupt_state;
    corrupt_state.magic = 0xDEADBEEF; // Wrong magic
    file.write(reinterpret_cast<const char*>(&corrupt_state), sizeof(corrupt_state));
    file.close();
    
    SequencerPersistentState loaded_state;
    REQUIRE_FALSE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
  }
  
  SECTION("Corrupted file with wrong version") {
    std::ofstream file(temp_file.path(), std::ios::binary);
    SequencerPersistentState corrupt_state;
    corrupt_state.version = 99; // Wrong version
    file.write(reinterpret_cast<const char*>(&corrupt_state), sizeof(corrupt_state));
    file.close();
    
    SequencerPersistentState loaded_state;
    REQUIRE_FALSE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
  }
  
  SECTION("Truncated file") {
    // Create a truncated file
    std::ofstream file(temp_file.path(), std::ios::binary);
    uint32_t partial_data = 0x12345678;
    file.write(reinterpret_cast<const char*>(&partial_data), sizeof(partial_data));
    file.close();
    
    SequencerPersistentState loaded_state;
    REQUIRE_FALSE(persister.load_from_file(temp_file.path().c_str(), loaded_state));
  }
  
  SECTION("Save to invalid path") {
    SequencerPersistentState state;
    REQUIRE_FALSE(persister.save_to_file("/invalid/path/that/does/not/exist.dat", state));
  }
}

} // namespace drum
