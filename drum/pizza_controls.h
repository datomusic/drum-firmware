#ifndef PIZZA_CONTROLS_H
#define PIZZA_CONTROLS_H

#include "drum_pizza_hardware.h"
#include "etl/array.h"
#include "etl/observer.h"
#include "musin/hal/analog_mux_scanner.h"
#include "musin/ui/analog_control.h"
#include "musin/ui/drumpad.h"
#include "musin/ui/keypad_hc138.h"
#include <cstddef>
#include <cstdint>
#include <optional>

#include "events.h"
#include "pico/time.h"

#include "config.h"

#include "musin/timing/internal_clock.h"

#include "message_router.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"

namespace drum {
class PizzaDisplay; // Forward declaration

template <size_t NumTracks, size_t NumSteps> class SequencerController;
using DefaultSequencerController =
    SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>;

class PizzaControls {
  friend class KeypadComponent;
  friend class DrumpadComponent;
  friend class AnalogControlComponent;
  friend class PlaybuttonComponent;

public:
  // Constructor takes essential shared resources and dependencies
  explicit PizzaControls(drum::PizzaDisplay &display_ref,
                         musin::timing::TempoHandler &tempo_handler_ref,
                         drum::DefaultSequencerController &sequencer_controller_ref,
                         drum::MessageRouter &message_router_ref, musin::Logger &logger_ref);

  PizzaControls(const PizzaControls &) = delete;
  PizzaControls &operator=(const PizzaControls &) = delete;

  void init();
  void update(absolute_time_t now);

  class KeypadComponent {
  public:
    explicit KeypadComponent(PizzaControls *parent_ptr);
    void init();
    void update();

  private:
    struct KeypadEventHandler : public etl::observer<musin::ui::KeypadEvent> {
      KeypadComponent *parent;
      const etl::array<uint8_t, KEYPAD_TOTAL_KEYS> &cc_map;
      const uint8_t midi_channel;

      constexpr KeypadEventHandler(KeypadComponent *p,
                                   const etl::array<uint8_t, KEYPAD_TOTAL_KEYS> &map,
                                   uint8_t channel)
          : parent(p), cc_map(map), midi_channel(channel) {
      }
      void notification(musin::ui::KeypadEvent event);

    private:
      void handle_sample_select(musin::ui::KeypadEvent event);
      void handle_sequencer_step(musin::ui::KeypadEvent event);
    };

    PizzaControls *parent_controls;
    musin::ui::Keypad_HC138<KEYPAD_ROWS, KEYPAD_COLS> keypad;
    static constexpr etl::array<uint8_t, KEYPAD_TOTAL_KEYS> keypad_cc_map = [] {
      etl::array<uint8_t, KEYPAD_TOTAL_KEYS> map{};
      for (size_t i = 0; i < KEYPAD_TOTAL_KEYS; ++i) {
        map[i] = (i <= config::keypad::MAX_CC_MAPPED_VALUE)
                     ? static_cast<uint8_t>(i)
                     : config::keypad::DEFAULT_CC_UNMAPPED_VALUE;
      }
      return map;
    }();
    KeypadEventHandler keypad_observer;
  };

  class DrumpadComponent {
  public:
    explicit DrumpadComponent(PizzaControls *parent_ptr);
    void init();
    void update();
    void select_note_for_pad(uint8_t pad_index, int8_t offset);
    uint8_t get_note_for_pad(uint8_t pad_index) const;
    [[nodiscard]] size_t get_num_drumpads() const {
      return config::NUM_DRUMPADS;
    }
    [[nodiscard]] const musin::ui::Drumpad &get_drumpad(size_t index) const {
      return drumpads[index];
    }

  private:
    struct DrumpadEventHandler : public etl::observer<musin::ui::DrumpadEvent> {
      DrumpadComponent *parent;
      musin::Logger &logger;

      constexpr DrumpadEventHandler(DrumpadComponent *p, musin::Logger &logger_ref)
          : parent(p), logger(logger_ref) {
      }
      void notification(musin::ui::DrumpadEvent event);
    };

    PizzaControls *parent_controls;
    std::array<musin::ui::Drumpad, config::NUM_DRUMPADS> drumpads;
    DrumpadEventHandler drumpad_observer;
    etl::array<musin::ui::RetriggerMode, config::NUM_DRUMPADS> _last_known_retrigger_mode_per_pad{};
  };

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
      void notification(musin::ui::DrumpadEvent event);
    };

    void update_playbutton();
    PizzaControls *parent_controls;
    musin::ui::Drumpad playbutton;
    PlaybuttonEventHandler playbutton_observer;
  };

  class AnalogControlComponent {
  public:
    explicit AnalogControlComponent(PizzaControls *parent_ptr);
    void init();
    void update(absolute_time_t now);

  private:
    struct AnalogControlEventHandler : public etl::observer<musin::ui::AnalogControlEvent> {
      AnalogControlComponent *parent;
      const uint16_t control_id;

      constexpr AnalogControlEventHandler(AnalogControlComponent *p, uint16_t id)
          : parent(p), control_id(id) {
      }
      void notification(musin::ui::AnalogControlEvent event);
    };

    PizzaControls *parent_controls;
    etl::array<musin::ui::AnalogControl, config::NUM_ANALOG_MUX_CONTROLS> mux_controls;
    etl::array<AnalogControlEventHandler, config::NUM_ANALOG_MUX_CONTROLS> control_observers;
    size_t _next_analog_control_to_update_idx = 0;

    // Smoothing for the filter knob
    float filter_target_value_{1.0f};  // Target value from the physical knob
    float filter_current_value_{1.0f}; // Smoothed value sent to the engine
    absolute_time_t last_smoothing_time_ = nil_time;
  };

private:
  // --- Shared Resources ---
  drum::PizzaDisplay &display;
  musin::timing::TempoHandler &_tempo_handler_ref;
  drum::DefaultSequencerController &_sequencer_controller_ref;
  drum::MessageRouter &_message_router_ref;
  musin::Logger &_logger_ref;

  // --- Owned Hardware Abstractions ---
  musin::hal::AnalogMuxScanner _scanner;

public:
  KeypadComponent keypad_component;
  DrumpadComponent drumpad_component;
  AnalogControlComponent analog_component;
  PlaybuttonComponent playbutton_component;

public:
  [[nodiscard]] bool is_running() const;
};

} // namespace drum
#endif // PIZZA_CONTROLS_H
