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

#include "step_sequencer.h"

namespace PizzaExample {
class PizzaDisplay;
}

class PizzaControls {
public:
  // Constructor now takes both display and sequencer references, using the namespace
  explicit PizzaControls(PizzaExample::PizzaDisplay &display_ref,
                         StepSequencer::Sequencer<4, 8> &sequencer_ref);

  // Prevent copying and assignment
  PizzaControls(const PizzaControls &) = delete;
  PizzaControls &operator=(const PizzaControls &) = delete;

  /**
   * @brief Initialize all input controls and attach observers.
   * Must be called once after PizzaDisplay is initialized.
   */
  void init();

  /**
   * @brief Update all input controls, process events, and request display updates.
   * Should be called periodically in the main loop.
   */
  void update();

private:
  // --- Private Helper Methods ---
  void update_drumpads();
  void select_note_for_pad(uint8_t pad_index, int8_t offset);
  uint32_t calculate_brightness_color(uint32_t base_color, uint16_t raw_value) const;
  float scale_raw_to_brightness(uint16_t raw_value) const;

  // --- Nested Observer Classes ---
  // These need access to PizzaControls members (like display, drumpad_note_numbers)

  struct AnalogControlEventHandler : public etl::observer<Musin::UI::AnalogControlEvent> {
    PizzaControls *parent;
    const uint16_t control_id;
    const uint8_t cc_number;
    const uint8_t midi_channel;

    constexpr AnalogControlEventHandler(PizzaControls *p, uint16_t id, uint8_t cc, uint8_t channel)
        : parent(p), control_id(id), cc_number(cc), midi_channel(channel) {
    }

    void notification(Musin::UI::AnalogControlEvent event) override;
  };

  struct KeypadEventHandler : public etl::observer<Musin::UI::KeypadEvent> {
    PizzaControls *parent;
    const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &cc_map;
    const uint8_t midi_channel;

    constexpr KeypadEventHandler(PizzaControls *p,
                                 const std::array<uint8_t, KEYPAD_TOTAL_KEYS> &map, uint8_t channel)
        : parent(p), cc_map(map), midi_channel(channel) {
    }

    void notification(Musin::UI::KeypadEvent event) override;
  };

  struct DrumpadEventHandler : public etl::observer<Musin::UI::DrumpadEvent> {
    PizzaControls *parent;
    const uint8_t pad_index;

    constexpr DrumpadEventHandler(PizzaControls *p, uint8_t index) : parent(p), pad_index(index) {
    }

    void notification(Musin::UI::DrumpadEvent event) override;
  };

  // --- Members ---
  PizzaExample::PizzaDisplay &display;
  StepSequencer::Sequencer<4, 8> &sequencer;

  // Keypad
  Musin::UI::Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad;
  static constexpr std::array<uint8_t, KEYPAD_TOTAL_KEYS> keypad_cc_map = [] {
    std::array<uint8_t, KEYPAD_TOTAL_KEYS> map{};
    for (size_t i = 0; i < KEYPAD_TOTAL_KEYS; ++i) {
      map[i] = (i <= 119) ? static_cast<uint8_t>(i) : 0;
    }
    return map;
  }();
  KeypadEventHandler keypad_observer;

  // Drumpads
  etl::array<Musin::HAL::AnalogInMux16, 4> drumpad_readers;
  etl::array<Musin::UI::Drumpad<Musin::HAL::AnalogInMux16>, 4> drumpads;
  etl::array<uint8_t, 4> drumpad_note_numbers;
  etl::array<DrumpadEventHandler, 4> drumpad_observers;

  // Analog Controls (Pots/Sliders)
  etl::array<Musin::UI::AnalogControl, 16> mux_controls;
  etl::array<AnalogControlEventHandler, 16> control_observers;
};

#endif // PIZZA_CONTROLS_H
