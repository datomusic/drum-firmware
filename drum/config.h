#ifndef DRUM_CONFIG_H
#define DRUM_CONFIG_H

#include "etl/array.h"
#include "etl/span.h"
#include <cstddef>
#include <cstdint>

// Forward declaration for pressure button config
namespace musin::ui {
struct PressureSensitiveButtonConfig;
}

namespace drum {
namespace config {

// General PizzaControls & Sequencer Configuration
constexpr size_t NUM_TRACKS = 4;
constexpr size_t NUM_STEPS_PER_TRACK = 8;
constexpr size_t NUM_DRUMPADS = 4;
constexpr size_t NUM_ANALOG_MUX_CONTROLS = 11;
constexpr uint32_t PROFILER_REPORT_INTERVAL_MS = 2000;
constexpr float DISPLAY_BRIGHTNESS_MAX_VALUE = 255.0f;
constexpr size_t MAX_PATH_LENGTH = 64;

// MIDI Configuration
constexpr uint8_t MIDI_IN_CHANNEL =
    10; // Input MIDI Channel (GM Percussion Standard)
constexpr uint8_t MIDI_OUT_CHANNEL =
    10; // Output MIDI Channel (GM Percussion Standard)
constexpr bool SEND_MIDI_CLOCK_WHEN_STOPPED_AS_MASTER = true;
constexpr bool SEND_SYNC_CLOCK_WHEN_STOPPED_AS_MASTER = true;
constexpr bool RETRIGGER_SYNC_ON_PLAYBUTTON = true;
constexpr bool IGNORE_MIDI_NOTE_OFF = true;
constexpr uint32_t COLOR_MIDI_CLOCK_LISTENER = 0x88FF55;

// SysEx Manufacturer and Device IDs
namespace sysex {
constexpr uint8_t MANUFACTURER_ID_0 = 0x00;
constexpr uint8_t MANUFACTURER_ID_1 = 0x22;
constexpr uint8_t MANUFACTURER_ID_2 = 0x01;
constexpr uint8_t DEVICE_ID = 0x65; // DRUM device ID

constexpr size_t MAX_FILENAME_LENGTH = 32;
constexpr uint32_t TIMEOUT_US = 5000000; // 5 seconds
// 146 * 7 bytes of raw data -> 146 * 8 = 1168 bytes of encoded data
constexpr size_t DECODED_CHUNK_SIZE = 1022;
} // namespace sysex

// Keypad Component Configuration
namespace keypad {
constexpr uint8_t _CHANNEL = 0;
constexpr uint32_t DEBOUNCE_TIME_MS = 10;
constexpr uint32_t POLL_INTERVAL_MS = 5;
constexpr uint32_t HOLD_TIME_MS = 1000;
constexpr uint32_t TAP_TIME_MS = 200;
constexpr uint8_t MAX_CC_MAPPED_VALUE = 119;
constexpr uint8_t DEFAULT_CC_UNMAPPED_VALUE = 0;
constexpr uint8_t SAMPLE_SELECT_START_COLUMN = 4;
constexpr uint8_t PREVIEW_NOTE_VELOCITY = 100;
constexpr uint8_t DEFAULT_STEP_VELOCITY = 100;
constexpr uint8_t STEP_VELOCITY_ON_HOLD = 27;
constexpr uint8_t STEP_VELOCITY_ON_TAP = 100;
} // namespace keypad

// Drumpad Component Configuration
namespace drumpad {
constexpr uint8_t DEFAULT_FALLBACK_NOTE = 36;
constexpr uint8_t RETRIGGER_VELOCITY = 100;

// Per-pad settings structure
struct DrumpadConfig {
  uint16_t noise_threshold;
  uint16_t trigger_threshold;
  uint16_t high_pressure_threshold;
  bool active_low;
  uint32_t debounce_time_us;
  uint32_t hold_time_us;
  uint64_t max_velocity_time_us;
  uint64_t min_velocity_time_us;
};

// Since all pads are physically identical, we can define a single configuration
constexpr DrumpadConfig default_drumpad_config = {.noise_threshold = 150,
                                                  .trigger_threshold = 800,
                                                  .high_pressure_threshold =
                                                      2500,
                                                  .active_low = true,
                                                  .debounce_time_us = 5000,
                                                  .hold_time_us = 50000,
                                                  .max_velocity_time_us = 50000,
                                                  .min_velocity_time_us = 100};

// Configuration for the play button, which is also a drumpad
constexpr DrumpadConfig play_button_config = {.noise_threshold = 150,
                                              .trigger_threshold = 800,
                                              .high_pressure_threshold = 0,
                                              .active_low = true,
                                              .debounce_time_us = 5000,
                                              .hold_time_us = 3000000,
                                              .max_velocity_time_us = 0,
                                              .min_velocity_time_us = 0};

// Create the array of configurations using the default for all pads
constexpr std::array<DrumpadConfig, NUM_DRUMPADS> drumpad_configs = {
    {default_drumpad_config, default_drumpad_config, default_drumpad_config,
     default_drumpad_config}};

} // namespace drumpad
// Linear MIDI note to sample slot mapping
// Each track has a contiguous range of MIDI notes that map directly to sample
// slots
struct TrackRange {
  uint8_t low_note;  // Inclusive lower bound
  uint8_t high_note; // Inclusive upper bound

  constexpr bool contains(uint8_t note) const {
    return note >= low_note && note <= high_note;
  }
};

// Default ranges - MIDI note N maps directly to sample slot N
constexpr etl::array<TrackRange, NUM_TRACKS> track_ranges = {{
    {30, 37}, // Track 0: notes 30-37 → sample slots 30-37
    {38, 45}, // Track 1: notes 38-45 → sample slots 38-45
    {46, 53}, // Track 2: notes 46-53 → sample slots 46-53
    {54, 61}  // Track 3: notes 54-61 → sample slots 54-61
}};

// Note Definitions with Colors
struct NoteDefinition {
  uint8_t midi_note_number;
  uint32_t color; // 0xRRGGBB
};

constexpr etl::array<NoteDefinition, 32> global_note_definitions = {
    {// Track 0 (notes 30-37)
     {30, 0xFF0000},
     {31, 0xFF0020},
     {32, 0xFF0040},
     {33, 0xFF0060},
     {34, 0xFF1010},
     {35, 0xFF1020},
     {36, 0xFF2040},
     {37, 0xFF2060},
     // Track 1 (notes 38-45)
     {38, 0x0000FF},
     {39, 0x0028FF},
     {40, 0x0050FF},
     {41, 0x0078FF},
     {42, 0x1010FF},
     {43, 0x1028FF},
     {44, 0x2050FF},
     {45, 0x3078FF},
     // Track 2 (notes 46-53)
     {46, 0x00FF00},
     {47, 0x00FF1E},
     {48, 0x00FF3C},
     {49, 0x00FF5A},
     {50, 0x10FF10},
     {51, 0x10FF1E},
     {52, 0x10FF3C},
     {53, 0x20FF5A},
     // Track 3 (notes 54-61)
     {54, 0xFFFF00},
     {55, 0xFFE100},
     {56, 0xFFC300},
     {57, 0xFFA500},
     {58, 0xFFFF20},
     {59, 0xFFE120},
     {60, 0xFFC320},
     {61, 0xFFA520}}};

// Analog Control Component Configuration
namespace analog_controls {
constexpr float FILTER_SMOOTHING_RATE =
    6.0f; // Lower is slower, higher is faster
constexpr float RANDOM_ACTIVATION_THRESHOLD = 0.1f;
constexpr float SWING_KNOB_CENTER_VALUE = 0.5f;
// Swing on/off handling uses a deadband around center; beyond this, swing is ON
constexpr float SWING_ON_OFF_DEADBAND =
    0.12f; // ~12% away from center to enable
constexpr uint8_t SWING_BASE_PERCENT = 50;
constexpr float SWING_PERCENT_SENSITIVITY = 33.0f;
constexpr float REPEAT_MODE_1_THRESHOLD = 0.3f;
constexpr float REPEAT_MODE_2_THRESHOLD = 0.7f;
constexpr uint32_t REPEAT_LENGTH_MODE_1 = 3;
constexpr uint32_t REPEAT_LENGTH_MODE_2 = 1;
// Hysteresis and debounce for REPEAT one-shot while stopped
constexpr float REPEAT_EDGE_ON_THRESHOLD =
    REPEAT_MODE_1_THRESHOLD + 0.05f; // Press threshold
constexpr float REPEAT_EDGE_OFF_THRESHOLD =
    REPEAT_MODE_1_THRESHOLD - 0.05f;             // Release threshold
constexpr uint32_t REPEAT_EDGE_DEBOUNCE_MS = 30; // Minimum time between edges
// Hysteresis and debounce for REPEAT while running
constexpr float REPEAT_MODE1_ENTER_THRESHOLD = REPEAT_MODE_1_THRESHOLD + 0.05f;
constexpr float REPEAT_MODE1_EXIT_THRESHOLD = REPEAT_MODE_1_THRESHOLD - 0.05f;
constexpr float REPEAT_MODE2_ENTER_THRESHOLD = REPEAT_MODE_2_THRESHOLD + 0.05f;
constexpr float REPEAT_MODE2_EXIT_THRESHOLD = REPEAT_MODE_2_THRESHOLD - 0.05f;
constexpr uint32_t REPEAT_RUNNING_DEBOUNCE_MS = 30;
constexpr float MIN_BPM_ADJUST = 60.0f;
constexpr float MAX_BPM_ADJUST = 360.0f;

} // namespace analog_controls

// Timing configuration (musical policies)
namespace timing {
// Fixed swing offset in 12 PPQN phases applied to swung steps only.
// Anchors remain at 0 and 6; the controller applies +SWING_OFFSET_PHASES
// to the next step when that step is marked as swung.
constexpr uint8_t SWING_OFFSET_PHASES = 2; // valid range: 1..5
static_assert(SWING_OFFSET_PHASES > 0 && SWING_OFFSET_PHASES < 6,
              "SWING_OFFSET_PHASES must be between 1 and 5 at 12 PPQN");
} // namespace timing

// PizzaControls specific
namespace main_controls {
constexpr uint8_t RETRIGGER_DIVISOR_FOR_DOUBLE_MODE = 2;
}

constexpr size_t MAX_NOTE_EVENT_OBSERVERS = 4;
constexpr size_t MAX_SYSEX_EVENT_OBSERVERS = 4;

// MessageRouter debounce configuration
namespace message_router {
constexpr uint32_t DEBOUNCE_TIME_MS =
    40; // Minimum time between triggers for the same note
} // namespace message_router

} // namespace config
} // namespace drum

#endif // DRUM_CONFIG_H
