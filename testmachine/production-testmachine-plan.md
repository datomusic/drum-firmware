# DRUM Test Machine Firmware Implementation Plan

## Overview

Transform the DRUM firmware into a factory test machine that validates hardware connectivity and functionality during manufacturing. The test machine responds to text-based serial commands and button presses, generating test signals on all interfaces (audio, MIDI, sync) and reporting results via LEDs and serial output.

## Requirements Summary

- **Purpose:** Factory testing during manufacturing
- **Trigger:** Serial commands + button presses (no auto-run on boot)
- **Protocol:** Text-based commands with JSON responses
- **Execution:** Sequential (one test at a time)
- **Priority Tests:** Audio tones, MIDI loopback/thru, Sync pulse generation/detection
- **Reporting:** LED visual status + detailed serial logs

## Architecture

### System Components

```
┌─────────────────────────────────────┐
│         Main Loop                   │
│  • Serial command processing        │
│  • Button event handling            │
│  • Active test update               │
│  • LED status display               │
└─────────────────────────────────────┘
           │
    ┌──────┼──────┐
    ▼      ▼      ▼
┌────────┬────────┬────────┐
│Command │ Test   │Display │
│Parser  │Manager │Manager │
└────────┴───┬────┴────────┘
             │
      ┌──────┴──────┐
      │  Test Suite │
      │ • Audio     │
      │ • MIDI      │
      │ • Sync      │
      └─────────────┘
```

### State Machine

- **Boot:** Initialize hardware, show welcome pattern (2s)
- **Ready:** Waiting for commands/button presses
- **Testing:** Running active test (non-interruptible)
- **TestComplete:** Showing pass/fail, auto-clear after 2s

## Serial Command Protocol

### Format
```
Command:  COMMAND [ARGS...]\n
Response: STATUS:MESSAGE\n
Results:  RESULT:{"test":"name","status":"pass",...}\n
```

### Core Commands

**System:**
```
PING                              → PONG:testmachine-v1.0.0
VERSION                           → VERSION:0.9.7
LIST_TESTS                        → TESTS:audio_sine,midi_loopback,...
```

**Priority Tests:**
```
TEST_AUDIO_SINE <hz> <ms>         → Generate sine wave
TEST_AUDIO_SQUARE <hz> <ms>       → Generate square wave
TEST_AUDIO_SWEEP <start> <end> <ms> → Frequency sweep
TEST_MIDI_LOOPBACK <timeout_ms>   → Send/receive MIDI notes
TEST_MIDI_THRU                    → Pass MIDI in→out for 5s
TEST_SYNC_OUT <pulses>            → Generate sync pulses
TEST_SYNC_IN <timeout_ms>         → Detect sync frequency
```

**Display:**
```
TEST_DISPLAY_PATTERN <0-4>        → Show LED test pattern
```

## LED Status Display

### Color Coding
- **BLUE:** Test ready/idle
- **CYAN:** Test running
- **GREEN:** Test passed
- **RED:** Test failed
- **MAGENTA:** Busy (animated)
- **WHITE:** System ready

### Display States
- **Ready:** Play button pulsing WHITE, test buttons dim BLUE
- **Running:** Active button CYAN, progress bar fills grid
- **Complete:** Active button GREEN/RED, auto-clear after 2s

## Implementation Structure

### New Files to Create

#### Test Framework (testmachine/test_framework/)
```
test_interface.h           Abstract Test base class
test_manager.h/cpp         Test orchestration
test_result.h              Result structures
test_registry.h/cpp        Static test registration
```

#### Audio Tests (testmachine/tests/audio/)
```
test_tone_generator.h/cpp  SampleReader for sine/square/sweep
audio_sine_test.h/cpp      440Hz sine wave test
audio_square_test.h/cpp    1kHz square wave test
audio_sweep_test.h/cpp     100Hz-5kHz frequency sweep
```

#### MIDI Tests (testmachine/tests/midi/)
```
midi_loopback_test.h/cpp   Send notes, verify loopback
midi_thru_test.h/cpp       Pass-through for 5 seconds
```

#### Sync Tests (testmachine/tests/sync/)
```
sync_out_test.h/cpp        Generate N pulses
sync_in_test.h/cpp         Detect frequency
```

#### Command Interface (testmachine/command/)
```
command_parser.h/cpp       Parse text commands
command_dispatcher.h/cpp   Route to test manager
response_formatter.h/cpp   JSON responses
```

#### Display Management (testmachine/display/)
```
test_display_manager.h/cpp LED visualization for tests
display_patterns.h         Predefined LED patterns
```

### Files to Modify

**testmachine/main.cpp**
- Remove: Sequencer setup, complex audio_engine
- Add: Command parser, test manager, simplified main loop

**testmachine/config.h**
- Add: Test timeouts, LED patterns, buffer sizes

**testmachine/CMakeLists.txt**
- Remove: sequencer_*, message_router, audio_engine, sample_repository, sysex_handler, save_timing_manager, configuration_manager
- Add: test_framework/*, tests/*, command/*, display/*
- Keep: midi_manager, pizza_display, pizza_controls, state machines

**testmachine/system_state.h**
- Replace states: Boot → Ready → Testing → TestComplete

**testmachine/state_implementations.h/cpp**
- Implement simplified test machine states

**testmachine/pizza_controls.cpp**
- Add button-to-test mapping table

**testmachine/ui/pizza_display.cpp**
- Add test status display methods

## Key Implementation Details

### 1. Test Interface
```cpp
enum class TestStatus {
  NOT_STARTED, RUNNING, PASSED, FAILED, TIMEOUT
};

struct TestResult {
  TestStatus status;
  etl::string<64> message;
  etl::map<etl::string<16>, int32_t, 8> metrics;
};

class Test {
public:
  virtual const char* get_name() const = 0;
  virtual void start() = 0;
  virtual void update(absolute_time_t now) = 0;
  virtual bool is_complete() const = 0;
  virtual TestResult get_result() const = 0;
  virtual void reset() = 0;
};
```

### 2. Audio Test Tone Generator

Implement `SampleReader` interface for waveform synthesis:

```cpp
class TestToneGenerator : public SampleReader {
private:
  WaveformType waveform_;     // SINE, SQUARE, SWEEP
  uint32_t frequency_hz_;
  uint32_t phase_accumulator_; // 8.24 fixed-point
  uint32_t phase_increment_;

  // 256-entry sine lookup table (512 bytes)
  static constexpr etl::array<int16_t, 256> SINE_TABLE = {...};

public:
  void set_frequency(uint32_t hz);
  void set_waveform(WaveformType type);

  // SampleReader interface
  uint32_t read_samples(AudioBlock &out) override;
};
```

**Key Details:**
- Sample rate: 44.1kHz
- Block size: 128 samples (AUDIO_BLOCK_SAMPLES)
- Phase accumulator: 32-bit fixed-point (8.24 format)
- Sine: Lookup table with linear interpolation
- Square: Phase MSB comparison

### 3. MIDI Loopback Test

```cpp
class MidiLoopbackTest : public Test {
private:
  struct TestMessage {
    uint8_t note, velocity;
    bool received;
  };

  etl::array<TestMessage, 16> test_messages_;
  uint8_t current_index_;
  absolute_time_t timeout_;

public:
  void start() override;  // Send 16 note messages
  void on_midi_received(uint8_t note, uint8_t vel);
  TestResult get_result() override;  // 100% pass
};
```

**Test Sequence:**
1. Send Note On messages (notes 36-51, vel 64)
2. 50ms delay between messages
3. 5 second timeout
4. Pass: All messages received correctly

### 4. Sync Pulse Tests

**Sync Out:**
```cpp
class SyncOutTest : public Test {
private:
  musin::timing::SyncOut &sync_out_;
  uint32_t target_pulses_;
  uint32_t pulses_sent_;
};
```

**Sync In:**
```cpp
class SyncInTest : public Test {
private:
  musin::timing::SyncIn &sync_in_;
  uint32_t pulses_detected_;
  absolute_time_t first_pulse_, last_pulse_;

  // Reports detected frequency in result
};
```

### 5. Main Loop Structure

```cpp
static testmachine::CommandParser command_parser;
static testmachine::TestManager test_manager;
static testmachine::TestDisplayManager display_manager;

// Static test instances
static testmachine::AudioSineTest audio_sine_test;
static testmachine::MidiLoopbackTest midi_loopback_test;
// ... all tests

void init() {
  // Initialize hardware
  pizza_display.init();
  midi_manager.init();

  // Register tests
  test_manager.register_test(&audio_sine_test);
  test_manager.register_test(&midi_loopback_test);
  // ... register all priority tests
}

while (true) {
  absolute_time_t now = get_absolute_time();

  // Process serial commands
  if (command_parser.has_command()) {
    Command cmd = command_parser.get_command();
    command_dispatcher.dispatch(cmd, test_manager);
  }

  // Update active test
  if (test_manager.has_active_test()) {
    test_manager.update(now);
    if (test_manager.is_test_complete()) {
      response_formatter.send_result(test_manager.get_result());
      display_manager.show_result(test_manager.get_result());
    }
  }

  // Update hardware
  pizza_controls.update(now);
  midi_manager.process_input();
  pizza_display.update(now);

  sleep_us(10);
}
```

## Implementation Phases

### Phase 1: Infrastructure (Priority 1)
**Goal:** Basic test framework and command processing

1. Create test framework interfaces (test_interface.h, test_manager, test_result)
2. Implement command parser and response formatter
3. Update main.cpp with new structure
4. Update CMakeLists.txt
5. **Verification:** Build succeeds, responds to PING/VERSION commands

**Files:**
- testmachine/test_framework/test_interface.h
- testmachine/test_framework/test_manager.h/cpp
- testmachine/test_framework/test_result.h
- testmachine/command/command_parser.h/cpp
- testmachine/command/response_formatter.h/cpp
- testmachine/main.cpp (major refactor)
- testmachine/CMakeLists.txt (update sources)

### Phase 2: Audio Tests (Priority 1)
**Goal:** Generate audio test tones

1. Implement TestToneGenerator with SINE_TABLE
2. Create AudioSineTest class
3. Create AudioSquareTest class
4. Create AudioSweepTest class
5. Integrate with AudioOutput pipeline
6. **Verification:** TEST_AUDIO_SINE 440 1000 produces audible tone

**Files:**
- testmachine/tests/audio/test_tone_generator.h/cpp
- testmachine/tests/audio/audio_sine_test.h/cpp
- testmachine/tests/audio/audio_square_test.h/cpp
- testmachine/tests/audio/audio_sweep_test.h/cpp

### Phase 3: MIDI Tests (Priority 1)
**Goal:** Validate MIDI I/O

1. Implement MidiLoopbackTest
2. Implement MidiThruTest
3. Hook into midi_manager callbacks
4. **Verification:** Loopback test passes with physical cable

**Files:**
- testmachine/tests/midi/midi_loopback_test.h/cpp
- testmachine/tests/midi/midi_thru_test.h/cpp
- testmachine/midi_manager.cpp (add test callbacks)

### Phase 4: Sync Tests (Priority 1)
**Goal:** Validate sync I/O

1. Implement SyncOutTest
2. Implement SyncInTest
3. Connect to existing musin sync drivers
4. **Verification:** Oscilloscope shows 2 PPQN pulses

**Files:**
- testmachine/tests/sync/sync_out_test.h/cpp
- testmachine/tests/sync/sync_in_test.h/cpp

### Phase 5: Display & Integration (Priority 2)
**Goal:** Complete visual feedback

1. Implement TestDisplayManager
2. Add button-to-test mappings
3. Implement LED status patterns
4. **Verification:** All tests show correct LED feedback

**Files:**
- testmachine/display/test_display_manager.h/cpp
- testmachine/display/display_patterns.h
- testmachine/pizza_controls.cpp (button mapping)
- testmachine/ui/pizza_display.cpp (status methods)

### Phase 6: State Machine (Priority 2)
**Goal:** Clean state transitions

1. Update system_state.h with test machine states
2. Implement Boot/Ready/Testing/TestComplete states
3. Integrate with test lifecycle
4. **Verification:** State transitions work correctly

**Files:**
- testmachine/system_state.h
- testmachine/state_implementations.h/cpp

## Critical Files Reference

### Must Read Before Implementation
1. **testmachine/main.cpp** - Current structure, integration points
2. **musin/audio/sample_reader.h** - Interface for tone generator
3. **musin/audio/audio_output.h** - Sample rate, block size constants
4. **musin/audio/block.h** - AudioBlock structure
5. **testmachine/midi_manager.h** - MIDI callback registration
6. **musin/timing/sync_in.h** - Pulse detection interface
7. **musin/timing/sync_out.h** - Pulse generation interface
8. **testmachine/ui/pizza_display.h** - LED control API
9. **drum/drum_pizza_hardware.h** - LED indices, hardware constants

### Reference During Implementation
10. **drum/audio_engine.cpp** - SampleReader usage example
11. **musin/audio/mixer.h** - Audio pipeline patterns
12. **musin/audio/dspinst.h** - Fixed-point math utilities
13. **testmachine/config.h** - Configuration pattern
14. **testmachine/CMakeLists.txt** - Build configuration

## Design Rationale

### Text Protocol
**Decision:** Use text commands with JSON responses
**Rationale:** Human-readable, easy debugging, manual testing possible, acceptable overhead for manufacturing test

### Sequential Testing
**Decision:** One test at a time, no parallelism
**Rationale:** Simpler state management, no resource conflicts, easier debugging, sufficient for factory testing

### Static Allocation
**Decision:** Allocate all test objects at compile time
**Rationale:** No dynamic memory, predictable footprint, fast switching, embedded best practices

### SampleReader Interface
**Decision:** Implement tone generator as SampleReader
**Rationale:** Integrates with existing audio pipeline, reuses AudioOutput, consistent with DRUM architecture

### 256-Entry Sine Table
**Decision:** Pre-computed lookup with linear interpolation
**Rationale:** 512 bytes ROM, excellent quality, deterministic performance, faster than runtime computation

## Memory Budget

### Flash
- Test framework: ~4 KB
- Audio tests (with sine table): ~8 KB
- MIDI/Sync tests: ~8 KB
- Command/Display: ~7 KB
- **Total new code:** ~27 KB
- **Total firmware:** ~120 KB (well within 2 MB limit)

### RAM
- Test instances: ~2 KB
- Command buffers: ~768 bytes
- Audio blocks: ~256 bytes
- **Total new RAM:** ~3 KB
- **Total RAM:** ~40 KB (well within 264 KB limit)

## Success Criteria

### Phase 1 Complete
- Firmware builds without errors
- Responds to PING command via serial
- Version reported correctly

### Phase 2 Complete
- TEST_AUDIO_SINE produces audible 440Hz tone
- TEST_AUDIO_SQUARE produces audible 1kHz square
- TEST_AUDIO_SWEEP sweeps 100Hz-5kHz over 2 seconds

### Phase 3 Complete
- TEST_MIDI_LOOPBACK passes with physical loopback cable
- TEST_MIDI_THRU passes MIDI messages through

### Phase 4 Complete
- TEST_SYNC_OUT generates measurable pulses (oscilloscope)
- TEST_SYNC_IN detects and reports pulse frequency

### Phases 5-6 Complete
- All tests show correct LED status (blue/cyan/green/red)
- Buttons trigger corresponding tests
- State transitions work smoothly

## Manufacturing Test Sequence

Recommended factory test workflow:
```
1. Connect device, power on
2. Open serial terminal
3. PING                       → Verify USB
4. TEST_DISPLAY_PATTERN 0     → All LEDs light
5. TEST_AUDIO_SINE 440 1000   → Verify audio out
6. Connect MIDI loopback cable
7. TEST_MIDI_LOOPBACK 5000    → Verify MIDI I/O
8. Connect oscilloscope to sync out
9. TEST_SYNC_OUT 10           → Verify sync output
10. All PASS → Unit marked GOOD
```
