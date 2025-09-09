#ifndef DRUM_UI_DISPLAY_MODE_H
#define DRUM_UI_DISPLAY_MODE_H

#include "drum/config.h"
#include "drum/sequencer_controller.h"
#include "drum/ui/color.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_handler.h"
#include "pico/time.h"
#include <functional>
#include <optional>

namespace drum {
class PizzaDisplay; // Forward declaration
}

namespace drum::ui {

// --- Abstract Base Class ---
class DisplayMode {
public:
  virtual ~DisplayMode() = default;
  virtual void draw(PizzaDisplay &display, absolute_time_t now) = 0;
  virtual void on_enter([[maybe_unused]] PizzaDisplay &display);
};

// --- Concrete Strategy for Sequencer Mode ---
class SequencerDisplayMode : public DisplayMode {
public:
  explicit SequencerDisplayMode(
      drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
          &sequencer_controller,
      musin::timing::TempoHandler &tempo_handler);
  void draw(PizzaDisplay &display, absolute_time_t now) override;

private:
  void draw_base_elements(PizzaDisplay &display, absolute_time_t now);
  void draw_animations(PizzaDisplay &display, absolute_time_t now);
  void draw_sequencer_state(PizzaDisplay &display, absolute_time_t now);
  void update_track_override_colors(PizzaDisplay &display);
  Color calculate_step_color(PizzaDisplay &display,
                             const musin::timing::Step &step) const;
  Color apply_pulsing_highlight(Color base_color, bool bright_phase) const;
  void sync_highlight_phase_with_step();
  bool is_highlight_bright(const PizzaDisplay &display) const;
  drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
      &_sequencer_controller_ref;
  musin::timing::TempoHandler &_tempo_handler_ref;
  std::optional<uint32_t> _last_synced_step_index{};
  bool _bright_phase_for_current_step = true;
};

// --- Concrete Strategy for File Transfer Mode ---
class FileTransferDisplayMode : public DisplayMode {
public:
  void draw(PizzaDisplay &display, absolute_time_t now) override;
  void on_enter(PizzaDisplay &display) override;

private:
  absolute_time_t _last_update_time = nil_time;
  uint8_t _chaser_position = 0;
};

// --- Concrete Strategy for Boot Animation ---
class BootAnimationMode : public DisplayMode {
public:
  explicit BootAnimationMode(
      drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
          &sequencer_controller);
  void draw(PizzaDisplay &display, absolute_time_t now) override;
  void on_enter(PizzaDisplay &display) override;

private:
  drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
      &_sequencer_controller_ref;
  uint8_t _boot_animation_track_index = 0;
  absolute_time_t _boot_animation_last_step_time{};
};

// --- Concrete Strategy for Sleep Mode ---
class SleepDisplayMode : public DisplayMode {
public:
  void draw(PizzaDisplay &display, absolute_time_t now) override;
  void on_enter(PizzaDisplay &display) override;
  void set_previous_mode(DisplayMode &previous_mode);

private:
  static constexpr uint32_t DIMMING_DURATION_MS = 500;
  static constexpr uint8_t MAX_BRIGHTNESS = 255;

  absolute_time_t _dimming_start_time = nil_time;
  std::optional<std::reference_wrapper<DisplayMode>> _previous_mode;
  uint8_t _original_brightness = MAX_BRIGHTNESS;

  uint8_t calculate_brightness(absolute_time_t now) const;
  float apply_ease_out_curve(float progress) const;
};

} // namespace drum::ui

#endif // DRUM_UI_DISPLAY_MODE_H
