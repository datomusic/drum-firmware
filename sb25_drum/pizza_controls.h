#ifndef PIZZA_CONTROLS_H
#define PIZZA_CONTROLS_H

#include "drum_pizza_hardware.h"
#include "etl/array.h"
#include "etl/observer.h"
#include "musin/hal/analog_in.h"
#include "musin/hal/analog_in.h" // For AnalogInMux16
#include "musin/ui/analog_control.h"
#include "musin/ui/drumpad.h"
#include "musin/ui/keypad_hc138.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "internal_clock.h"
#include "step_sequencer.h"

namespace PizzaExample {
class PizzaDisplay; // Forward declaration
}
namespace StepSequencer {
template <size_t NumTracks, size_t NumSteps> class SequencerController;
using DefaultSequencerController = SequencerController<4, 8>;
} // namespace StepSequencer

class PizzaControls {
public:
  // Constructor takes essential shared resources and dependencies
  explicit PizzaControls(PizzaExample::PizzaDisplay &display_ref,
                         StepSequencer::Sequencer<4, 8> &sequencer_ref,
                         Clock::InternalClock &clock_ref,
                         StepSequencer::DefaultSequencerController &sequencer_controller_ref);

  PizzaControls(const PizzaControls &) = delete;
  PizzaControls &operator=(const PizzaControls &) = delete;

  void init();
  void update();

  // --- Nested Component Definitions ---

  // --- Keypad Component ---
  class KeypadComponent {
  public:
    explicit KeypadComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct KeypadEventHandler : public etl::observer<Musin::UI::KeypadEvent> {
      KeypadComponent *parent; // Changed to KeypadComponent pointer
      const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &cc_map;
      const uint8_t midi_channel;

      constexpr KeypadEventHandler(KeypadComponent *p,
                                   const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &map,
                                   uint8_t channel)
          : parent(p), cc_map(map), midi_channel(channel) {
      }
      void notification(Musin::UI::KeypadEvent event) override;
    };

    PizzaControls *parent_controls; // Pointer back to the main class
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
  class DrumpadComponent {
  public:
    explicit DrumpadComponent(PizzaControls *parent_ptr);
    void init();
    void update();
    void select_note_for_pad(uint8_t pad_index, int8_t offset);
    uint8_t get_note_for_pad(uint8_t pad_index) const;

  private:
    struct DrumpadEventHandler : public etl::observer<Musin::UI::DrumpadEvent> {
      DrumpadComponent *parent; // Changed to DrumpadComponent pointer
      const uint8_t pad_index;

      constexpr DrumpadEventHandler(DrumpadComponent *p, uint8_t index)
          : parent(p), pad_index(index) {
      }
      void notification(Musin::UI::DrumpadEvent event) override;
    };

    void update_drumpads(); // Moved from PizzaControls
    uint32_t calculate_brightness_color(uint32_t base_color, uint16_t raw_value) const;
    float scale_raw_to_brightness(uint16_t raw_value) const;

    PizzaControls *parent_controls; // Pointer back to the main class
    etl::array<Musin::HAL::AnalogInMux16, 4> drumpad_readers;
    etl::array<Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>, 4> drumpads;
    etl::array<uint8_t, 4> drumpad_note_numbers;
    etl::array<DrumpadEventHandler, 4> drumpad_observers;
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

    void update_playbutton();       // Moved from PizzaControls
    PizzaControls *parent_controls; // Pointer back to the main class
    Musin::HAL::AnalogInMux16 playbutton_reader;
    Musin::UI::Drumpad<Musin::HAL::AnalogInMux16> playbutton;
    PlaybuttonEventHandler playbutton_observer;
  };

  // --- Analog Control Component ---
  class AnalogControlComponent {
  public:
    explicit AnalogControlComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct AnalogControlEventHandler : public etl::observer<Musin::UI::AnalogControlEvent> {
      AnalogControlComponent *parent;
      const uint16_t control_id;

      constexpr AnalogControlEventHandler(AnalogControlComponent *p, uint16_t id)
          : parent(p), control_id(id) {
      }
      void notification(Musin::UI::AnalogControlEvent event) override;
    };

    PizzaControls *parent_controls; // Pointer back to the main class
    etl::array<Musin::UI::AnalogControl, 16> mux_controls;
    etl::array<AnalogControlEventHandler, 16> control_observers;
  };

private:
  // --- Shared Resources ---
  PizzaExample::PizzaDisplay &display;
  StepSequencer::Sequencer<4, 8> &sequencer;
  Clock::InternalClock &_internal_clock;
  StepSequencer::DefaultSequencerController &_sequencer_controller_ref;

  // --- Components ---
  KeypadComponent keypad_component;
  DrumpadComponent drumpad_component;
  AnalogControlComponent analog_component;

  PlaybuttonComponent playbutton_component;
};

#endif // PIZZA_CONTROLS_H
