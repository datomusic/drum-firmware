#ifndef DRUM_CONFIG_H
#define DRUM_CONFIG_H

#include <cstddef>
#include <cstdint>
#include "etl/array.h"
#include "etl/span.h"

namespace drum {
namespace config {

// General PizzaControls & Sequencer Configuration
constexpr size_t NUM_TRACKS = 4;
constexpr size_t NUM_STEPS_PER_TRACK = 8;
constexpr size_t NUM_DRUMPADS = 4;
constexpr size_t NUM_ANALOG_MUX_CONTROLS = 11;
constexpr uint32_t PROFILER_REPORT_INTERVAL_MS = 2000;
constexpr float DISPLAY_BRIGHTNESS_MAX_VALUE = 255.0f;

// Keypad Component Configuration
namespace keypad {
    constexpr uint8_t MIDI_CHANNEL = 0;
    constexpr uint32_t DEBOUNCE_TIME_MS = 10;
    constexpr uint32_t POLL_INTERVAL_MS = 5;
    constexpr uint32_t HOLD_TIME_MS = 1000;
    constexpr uint8_t MAX_CC_MAPPED_VALUE = 119;
    constexpr uint8_t DEFAULT_CC_UNMAPPED_VALUE = 0;
    constexpr uint8_t SAMPLE_SELECT_START_COLUMN = 4;
    constexpr uint8_t PREVIEW_NOTE_VELOCITY = 100;
    constexpr uint8_t DEFAULT_STEP_VELOCITY = 100;
    constexpr uint8_t MAX_STEP_VELOCITY_ON_HOLD = 127;
}

// Drumpad Component Configuration
namespace drumpad {
    constexpr uint8_t DEFAULT_FALLBACK_NOTE = 36;
    constexpr uint8_t RETRIGGER_VELOCITY = 100;

    constexpr uint32_t DEBOUNCE_PRESS_MS = 50U;
    constexpr uint32_t DEBOUNCE_RELEASE_MS = 250U;
    constexpr uint32_t HOLD_THRESHOLD_MS = 150U;
    constexpr uint32_t HOLD_REPEAT_DELAY_MS = 1500U;
    constexpr uint32_t HOLD_REPEAT_INTERVAL_MS = 100U;
    constexpr uint32_t MIN_PRESSURE_VALUE = 800U;
    constexpr uint32_t MAX_PRESSURE_VALUE = 1000U;
    constexpr uint32_t MIN_VELOCITY_VALUE = 5000U;
    constexpr uint32_t MAX_VELOCITY_VALUE = 200000U;

    constexpr etl::array<uint8_t, 6> track_0_notes = {{0, 1, 2, 3, 5, 7}};
    constexpr etl::array<uint8_t, 7> track_1_notes = {{10, 11, 13, 14, 15, 8, 9 }};
    constexpr etl::array<uint8_t, 5> track_2_notes = {{16, 17, 19, 20, 21}};
    constexpr etl::array<uint8_t, 7> track_3_notes = {{24, 26, 27, 28, 29, 30, 31}};

    constexpr etl::array<etl::span<const uint8_t>, NUM_DRUMPADS> track_note_ranges = {{
        etl::span<const uint8_t>(track_0_notes),
        etl::span<const uint8_t>(track_1_notes),
        etl::span<const uint8_t>(track_2_notes),
        etl::span<const uint8_t>(track_3_notes)
    }};
}

// Analog Control Component Configuration
namespace analog_controls {
    constexpr float RANDOM_ACTIVATION_THRESHOLD = 0.1f;
    constexpr float SWING_KNOB_CENTER_VALUE = 0.5f;
    constexpr uint8_t SWING_BASE_PERCENT = 50;
    constexpr float SWING_PERCENT_SENSITIVITY = 33.0f;
    constexpr float REPEAT_MODE_1_THRESHOLD = 0.3f;
    constexpr float REPEAT_MODE_2_THRESHOLD = 0.7f;
    constexpr uint32_t REPEAT_LENGTH_MODE_1 = 3;
    constexpr uint32_t REPEAT_LENGTH_MODE_2 = 1;
    constexpr float MIN_BPM_ADJUST = 30.0f;
    constexpr float MAX_BPM_ADJUST = 360.0f;
}

// PizzaControls specific
namespace main_controls {
    constexpr uint8_t RETRIGGER_DIVISOR_FOR_DOUBLE_MODE = 2;
}

} // namespace config
} // namespace drum

#endif // DRUM_CONFIG_H
