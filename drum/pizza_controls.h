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

#include "musin/timing/internal_clock.h"

#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"
#include "sound_router.h" // Added

namespace drum {
class PizzaDisplay; // Forward declaration
}
namespace StepSequencer {
template <size_t NumTracks, size_t NumSteps> class SequencerController;
using DefaultSequencerController = SequencerController<4, 8>;
} // namespace StepSequencer

class PizzaControls : public etl::observer<Musin::Timing::TempoEvent> {
public:
  // Constructor takes essential shared resources and dependencies
  explicit PizzaControls(drum::PizzaDisplay &display_ref,
                         Musin::Timing::Sequencer<4, 8> &sequencer_ref,
                         Musin::Timing::InternalClock &clock_ref,
                         Musin::Timing::TempoHandler &tempo_handler_ref,
                         StepSequencer::DefaultSequencerController &sequencer_controller_ref,
                         drum::SoundRouter &sound_router_ref); // Added sound_router_ref

  PizzaControls(const PizzaControls &) = delete;
  PizzaControls &operator=(const PizzaControls &) = delete;

  void init();
  void update();
  void notification(Musin::Timing::TempoEvent event) override;

  // --- Nested Component Definitions ---

  // --- Keypad Component ---
  class KeypadComponent {
  public:
    explicit KeypadComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct KeypadEventHandler : public etl::observer<Musin::UI::KeypadEvent> {
      KeypadComponent *parent;
      const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &cc_map;
      const uint8_t midi_channel;

      constexpr KeypadEventHandler(KeypadComponent *p,
                                   const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &map,
                                   uint8_t channel)
          : parent(p), cc_map(map), midi_channel(channel) {
      }
      void notification(Musin::UI::KeypadEvent event) override;
    };

    PizzaControls *parent_controls;
    Musin::UI::Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad;
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
  class DrumpadComponent : public etl::observable<etl::observer<drum::Events::NoteEvent>, 1> {
  public:
    explicit DrumpadComponent(PizzaControls *parent_ptr); // Removed sound_router
    void init();
    void update();
    void select_note_for_pad(uint8_t pad_index, int8_t offset);
    void trigger_fade(uint8_t pad_index); // New method to start the fade effect
    uint8_t get_note_for_pad(uint8_t pad_index) const;

  private:
    struct DrumpadEventHandler : public etl::observer<Musin::UI::DrumpadEvent> {
      DrumpadComponent *parent;
      const uint8_t pad_index;

      constexpr DrumpadEventHandler(DrumpadComponent *p, uint8_t index) // Removed sr
          : parent(p), pad_index(index) {                               // Removed _sound_router(sr)
      }
      void notification(Musin::UI::DrumpadEvent event) override;
    };

    void update_drumpads();

    PizzaControls *parent_controls;
    // drum::SoundRouter &_sound_router; // Removed
    etl::array<Musin::HAL::AnalogInMux16, 4> drumpad_readers;
    etl::array<Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>, 4> drumpads;
    etl::array<uint8_t, 4> drumpad_note_numbers;
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
    struct PlaybuttonEventHandler : public etl::observer<Musin::UI::DrumpadEvent> {
      PlaybuttonComponent *parent;

      constexpr PlaybuttonEventHandler(PlaybuttonComponent *p) : parent(p) {
      }
      void notification(Musin::UI::DrumpadEvent event) override;
    };

    void update_playbutton();
    PizzaControls *parent_controls;
    Musin::HAL::AnalogInMux16 playbutton_reader;
    Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> playbutton;
    PlaybuttonEventHandler playbutton_observer;
  };

  // --- Analog Control Component ---
  class AnalogControlComponent {
  public:
    explicit AnalogControlComponent(PizzaControls *parent_ptr,
                                    drum::SoundRouter &sound_router); // Added sound_router
    void init();
    void update();

  private:
    struct AnalogControlEventHandler : public etl::observer<Musin::UI::AnalogControlEvent> {
      AnalogControlComponent *parent;
      const uint16_t control_id;
      drum::SoundRouter &_sound_router; // Added

      constexpr AnalogControlEventHandler(AnalogControlComponent *p, uint16_t id,
                                          drum::SoundRouter &sr) // Added sr
          : parent(p), control_id(id), _sound_router(sr) {       // Added _sound_router(sr)
      }
      void notification(Musin::UI::AnalogControlEvent event) override;
    };

    PizzaControls *parent_controls;
    drum::SoundRouter &_sound_router; // Added
    etl::array<Musin::UI::AnalogControl, 16> mux_controls;
    etl::array<AnalogControlEventHandler, 16> control_observers;
  };

private:
  // --- Shared Resources ---
  drum::PizzaDisplay &display;
  Musin::Timing::Sequencer<4, 8> &sequencer;
  Musin::Timing::InternalClock &_internal_clock;
  Musin::Timing::TempoHandler &_tempo_handler_ref;
  StepSequencer::DefaultSequencerController &_sequencer_controller_ref;
  drum::SoundRouter &_sound_router_ref; // Added

public: // Make components public for access from SequencerController etc.
  // --- Components ---
  KeypadComponent keypad_component;
  DrumpadComponent drumpad_component;
  AnalogControlComponent analog_component;
  PlaybuttonComponent playbutton_component;

  // --- Internal State ---
  uint32_t _clock_tick_counter = 0;       // Counter for LED pulsing when stopped
  float _stopped_highlight_factor = 0.0f; // Brightness factor for LED pulse (0.0-1.0)

public:                                  // Add getters for state needed by display drawing
  [[nodiscard]] bool is_running() const; // Moved definition to .cpp
  [[nodiscard]] float get_stopped_highlight_factor() const {
    return _stopped_highlight_factor; // This one is simple, can stay inline
  }
};

#endif // PIZZA_CONTROLS_H
