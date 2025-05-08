#ifndef PIZZA_CONTROLS_H
#define PIZZA_CONTROLS_H

#include "drum_pizza_hardware.h"
#include "etl/array.h"
#include "etl/observer.h"
#include "musin/hal/analog_in.h"
#include "musin/ui/analog_control.h"
#include "musin/ui/drumpad.h"
#include "musin/ui/keypad_hc138.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "events.h"
#include "pico/time.h"

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
    constexpr float MIN_FADE_BRIGHTNESS_FACTOR = 0.1f;
    constexpr uint32_t FADE_DURATION_MS = 150;
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


#include "musin/hal/debug_utils.h"
#include "musin/timing/internal_clock.h"

#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"
#include "musin/timing/tempo_multiplier.h"
#include "sound_router.h"

namespace drum {
class PizzaDisplay; // Forward declaration

template <size_t NumTracks, size_t NumSteps> class SequencerController;
using DefaultSequencerController = SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>;

class PizzaControls
    : public etl::observer<musin::timing::TempoEvent>,
      public etl::observer<musin::timing::SequencerTickEvent>, // Added
      public etl::observer<drum::Events::NoteEvent> {
public:
  // Constructor takes essential shared resources and dependencies
  explicit PizzaControls(drum::PizzaDisplay &display_ref,
                         musin::timing::Sequencer<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &sequencer_ref,
                         musin::timing::InternalClock &clock_ref,
                         musin::timing::TempoHandler &tempo_handler_ref,
                         musin::timing::TempoMultiplier &tempo_multiplier_ref, // Added
                         drum::DefaultSequencerController &sequencer_controller_ref,
                         drum::SoundRouter &sound_router_ref);

  PizzaControls(const PizzaControls &) = delete;
  PizzaControls &operator=(const PizzaControls &) = delete;

  void init();
  void update();
  void notification(musin::timing::TempoEvent event) override;
  void notification(musin::timing::SequencerTickEvent event) override; // Added
  void notification(drum::Events::NoteEvent event) override;

  void refresh_sequencer_display();

  // --- Nested Component Definitions ---

  // --- Keypad Component ---
  class KeypadComponent {
  public:
    explicit KeypadComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct KeypadEventHandler : public etl::observer<musin::ui::KeypadEvent> {
      KeypadComponent *parent;
      const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &cc_map;
      const uint8_t midi_channel;

      constexpr KeypadEventHandler(KeypadComponent *p,
                                   const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &map,
                                   uint8_t channel)
          : parent(p), cc_map(map), midi_channel(channel) {
      }
      void notification(musin::ui::KeypadEvent event) override;
    };

    PizzaControls *parent_controls;
    musin::ui::Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad;
    static constexpr std::array<uint8_t, KEYPAD_TOTAL_KEYS> keypad_cc_map = [] {
      std::array<uint8_t, KEYPAD_TOTAL_KEYS> map{};
      for (size_t i = 0; i < KEYPAD_TOTAL_KEYS; ++i) {
        map[i] = (i <= config::keypad::MAX_CC_MAPPED_VALUE) ? static_cast<uint8_t>(i) : config::keypad::DEFAULT_CC_UNMAPPED_VALUE;
      }
      return map;
    }();
    KeypadEventHandler keypad_observer;
  };

  // --- Drumpad Component ---
  // Now observes DrumpadEvents and emits NoteEvents
  class DrumpadComponent {
  public:
    explicit DrumpadComponent(PizzaControls *parent_ptr); // Removed sound_router
    void init();
    void update();
    void select_note_for_pad(uint8_t pad_index, int8_t offset);
    void trigger_fade(uint8_t pad_index); // New method to start the fade effect
    uint8_t get_note_for_pad(uint8_t pad_index) const;
    [[nodiscard]] size_t get_num_drumpads() const {
      return config::NUM_DRUMPADS;
    }
    [[nodiscard]] bool is_pad_pressed(uint8_t pad_index) const;
    [[nodiscard]] const musin::ui::Drumpad<musin::hal::AnalogInMux16> &
    get_drumpad(size_t index) const {
      return drumpads[index];
    }

    static constexpr etl::array<uint8_t, 6> drumpad_0_notes = {{0, 1, 2, 3, 5, 7}};
    static constexpr etl::array<uint8_t, 7> drumpad_1_notes = {{10, 11, 13, 14, 15, 8, 9 }};
    static constexpr etl::array<uint8_t, 5> drumpad_2_notes = {{16, 17, 19, 20, 21}};
    static constexpr etl::array<uint8_t, 7> drumpad_3_notes = {{24, 26, 27, 28, 29, 30, 31}};

    static constexpr etl::array<etl::span<const uint8_t>, config::NUM_DRUMPADS> drumpad_note_ranges = {{
        etl::span<const uint8_t>(drumpad_0_notes),
        etl::span<const uint8_t>(drumpad_1_notes),
        etl::span<const uint8_t>(drumpad_2_notes),
        etl::span<const uint8_t>(drumpad_3_notes)
    }};

  private:
    struct DrumpadEventHandler : public etl::observer<musin::ui::DrumpadEvent> {
      DrumpadComponent *parent;
      const uint8_t pad_index;

      constexpr DrumpadEventHandler(DrumpadComponent *p, uint8_t index) // Removed sr
          : parent(p), pad_index(index) {                               // Removed _sound_router(sr)
      }
      void notification(musin::ui::DrumpadEvent event) override;
    };

    void update_drumpads();

    PizzaControls *parent_controls;
    // drum::SoundRouter &_sound_router; // Removed
    etl::array<musin::hal::AnalogInMux16, config::NUM_DRUMPADS> drumpad_readers;
    etl::array<musin::ui::Drumpad<musin::hal::AnalogInMux16>, config::NUM_DRUMPADS> drumpads;
    etl::array<bool, config::NUM_DRUMPADS> _pad_pressed_state{};
    etl::array<absolute_time_t, config::NUM_DRUMPADS> _fade_start_time; // Track fade start time per pad
    etl::array<DrumpadEventHandler, config::NUM_DRUMPADS>
        drumpad_observers; // Declared after _fade_start_time to match init order
    etl::array<musin::ui::RetriggerMode, config::NUM_DRUMPADS> _last_known_retrigger_mode_per_pad{};
  };

  // --- Playbutton Component ---
  class PlaybuttonComponent {
  public:
    explicit PlaybuttonComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct PlaybuttonEventHandler : public etl::observer<musin::ui::DrumpadEvent> {
      PlaybuttonComponent *parent;

      constexpr PlaybuttonEventHandler(PlaybuttonComponent *p) : parent(p) {
      }
      void notification(musin::ui::DrumpadEvent event) override;
    };

    void update_playbutton();
    PizzaControls *parent_controls;
    musin::hal::AnalogInMux16 playbutton_reader;
    musin::ui::Drumpad<musin::hal::AnalogInMux16> playbutton;
    PlaybuttonEventHandler playbutton_observer;
  };

  // --- Analog Control Component ---
  class AnalogControlComponent {
  public:
    explicit AnalogControlComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct AnalogControlEventHandler : public etl::observer<musin::ui::AnalogControlEvent> {
      AnalogControlComponent *parent;
      const uint16_t control_id;

      constexpr AnalogControlEventHandler(AnalogControlComponent *p, uint16_t id)
          : parent(p), control_id(id) {
      }
      void notification(musin::ui::AnalogControlEvent event) override;
    };

    PizzaControls *parent_controls;
    etl::array<musin::ui::AnalogControl, config::NUM_ANALOG_MUX_CONTROLS> mux_controls;
    etl::array<AnalogControlEventHandler, config::NUM_ANALOG_MUX_CONTROLS> control_observers;
    size_t _next_analog_control_to_update_idx = 0;
  };

private:
  // --- Shared Resources ---
  drum::PizzaDisplay &display;
  musin::timing::Sequencer<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &sequencer;
  musin::timing::InternalClock &_internal_clock;
  musin::timing::TempoHandler &_tempo_handler_ref;
  musin::timing::TempoMultiplier &_tempo_multiplier_ref;
  drum::DefaultSequencerController &_sequencer_controller_ref;
  drum::SoundRouter &_sound_router_ref;

public: // Make components public for access from SequencerController etc.
  // --- Components ---
  KeypadComponent keypad_component;
  DrumpadComponent drumpad_component;
  AnalogControlComponent analog_component;
  PlaybuttonComponent playbutton_component;

  // --- Internal State ---
  uint32_t _clock_tick_counter = 0;       // Counter for LED pulsing when stopped
  float _stopped_highlight_factor = 0.0f; // Brightness factor for LED pulse (0.0-1.0)
  musin::hal::DebugUtils::SectionProfiler<4> _profiler;

  enum class ProfileSection {
    KEYPAD_UPDATE,
    DRUMPAD_UPDATE,
    ANALOG_UPDATE,
    PLAYBUTTON_UPDATE
  };

public:                                  
  [[nodiscard]] bool is_running() const;
  [[nodiscard]] float get_stopped_highlight_factor() const {
    return _stopped_highlight_factor;
  }
};

} // namespace drum
#endif // PIZZA_CONTROLS_H
