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

#include "musin/hal/debug_utils.h"
#include "musin/timing/internal_clock.h"

#include "musin/timing/sequencer_tick_event.h" // Added for SequencerTickEvent
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"
#include "musin/timing/tempo_multiplier.h"
#include "sound_router.h"

namespace drum {
class PizzaDisplay; // Forward declaration

template <size_t NumTracks, size_t NumSteps> class SequencerController;
using DefaultSequencerController = SequencerController<4, 8>;

class PizzaControls
    : public etl::observer<musin::timing::TempoEvent>,
      public etl::observer<musin::timing::SequencerTickEvent>, // Added SequencerTickEvent
      public etl::observer<drum::Events::NoteEvent> {
public:
  // Constructor takes essential shared resources and dependencies
  explicit PizzaControls(drum::PizzaDisplay &display_ref,
                         musin::timing::Sequencer<4, 8> &sequencer_ref,
                         musin::timing::InternalClock &clock_ref,
                         musin::timing::TempoHandler &tempo_handler_ref,
                         musin::timing::TempoMultiplier &tempo_multiplier_ref, // Added
                         drum::DefaultSequencerController &sequencer_controller_ref,
                         drum::SoundRouter &sound_router_ref);

  PizzaControls(const PizzaControls &) = delete;
  PizzaControls &operator=(const PizzaControls &) = delete;

  void init();
  void update();
  void notification(musin::timing::TempoEvent event);
  void notification(musin::timing::SequencerTickEvent event); // Added for SequencerTickEvent
  void notification(drum::Events::NoteEvent event);

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
        map[i] = (i <= 119) ? static_cast<uint8_t>(i) : 0;
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
      return drumpads.size();
    }
    [[nodiscard]] bool is_pad_pressed(uint8_t pad_index) const;
    [[nodiscard]] const musin::ui::Drumpad<musin::hal::AnalogInMux16> &
    get_drumpad(size_t index) const {
      // Consider adding bounds check: hard_assert(index < drumpads.size());
      return drumpads[index];
    }

    struct NoteRange {
      uint8_t min_note;
      uint8_t max_note;
    };
    static constexpr etl::array<NoteRange, 4> drumpad_note_ranges = {{
        {0, 7},   // Pad 0
        {8, 15},  // Pad 1
        {16, 23}, // Pad 2
        {24, 31}  // Pad 3
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
    etl::array<musin::hal::AnalogInMux16, 4> drumpad_readers;
    etl::array<musin::ui::Drumpad<musin::hal::AnalogInMux16>, 4> drumpads;
    etl::array<uint8_t, 4> drumpad_note_numbers;
    etl::array<bool, 4> _pad_pressed_state{false, false, false, false};
    etl::array<absolute_time_t, 4> _fade_start_time; // Track fade start time per pad
    etl::array<DrumpadEventHandler, 4>
        drumpad_observers; // Declared after _fade_start_time to match init order
    static constexpr float MIN_FADE_BRIGHTNESS_FACTOR =
        0.1f; // Brightness factor at the start of fade (10%)
    static constexpr uint32_t FADE_DURATION_MS = 150; // Fade duration
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
    etl::array<musin::ui::AnalogControl, 11> mux_controls;
    etl::array<AnalogControlEventHandler, 11> control_observers;
    size_t _next_analog_control_to_update_idx = 0;
  };

private:
  // --- Shared Resources ---
  drum::PizzaDisplay &display;
  musin::timing::Sequencer<4, 8> &sequencer;
  musin::timing::InternalClock &_internal_clock;
  musin::timing::TempoHandler &_tempo_handler_ref;
  musin::timing::TempoMultiplier &_tempo_multiplier_ref; // Added
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
  uint32_t _sub_step_tick_counter = 0;    // Counter for TempoEvents within a SequencerTick
  float _stopped_highlight_factor = 0.0f; // Brightness factor for LED pulse (0.0-1.0)
  musin::hal::DebugUtils::SectionProfiler<4> _profiler;

  enum class ProfileSection {
    KEYPAD_UPDATE,
    DRUMPAD_UPDATE,
    ANALOG_UPDATE,
    PLAYBUTTON_UPDATE
  };

public:                                  // Add getters for state needed by display drawing
  [[nodiscard]] bool is_running() const; // Moved definition to .cpp
  [[nodiscard]] float get_stopped_highlight_factor() const {
    return _stopped_highlight_factor; // This one is simple, can stay inline
  }
};

} // namespace drum
#endif // PIZZA_CONTROLS_H
