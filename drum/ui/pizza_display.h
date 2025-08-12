#ifndef PIZZA_DISPLAY_H
#define PIZZA_DISPLAY_H

#include "drum/drum_pizza_hardware.h"
#include "drum/ui/color.h"
#include "etl/array.h"
#include "musin/drivers/ws2812-dma.h"
#include "pico/time.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "drum/config.h"
#include "drum/events.h"
#include "drum/sequencer_controller.h"
#include "drum/ui/display_mode.h"
#include "etl/observer.h"
#include "musin/timing/step_sequencer.h"
#include "musin/timing/tempo_event.h"
#include "musin/timing/tempo_handler.h"

namespace drum {

namespace ui {
class SequencerDisplayMode;
class FileTransferDisplayMode;
class BootAnimationMode;
} // namespace ui

class PizzaDisplay
    : public etl::observer<musin::timing::TempoEvent>,
      public etl::observer<drum::Events::NoteEvent>,
      public etl::observer<drum::Events::SysExTransferStateChangeEvent>,
      public etl::observer<drum::Events::ParameterChangeEvent> {
public:
  friend class ui::SequencerDisplayMode;
  friend class ui::FileTransferDisplayMode;
  friend class ui::BootAnimationMode;

  static constexpr size_t SEQUENCER_TRACKS_DISPLAYED = 4;
  static constexpr size_t SEQUENCER_STEPS_DISPLAYED = 8;
  static constexpr float MIN_FADE_BRIGHTNESS_FACTOR = 0.1f;
  static constexpr uint32_t FADE_DURATION_MS = 150;
  static constexpr uint16_t VELOCITY_TO_BRIGHTNESS_SCALE = 2;
  static constexpr uint8_t HIGHLIGHT_BLEND_AMOUNT = 100;
  static constexpr Color COLOR_WHITE = Color(0xFFFFFF);
  static constexpr Color COLOR_GREEN = Color(0x00FF00);
  static constexpr uint16_t INTENSITY_TO_BRIGHTNESS_SCALE = 2;
  static constexpr uint8_t MAX_BRIGHTNESS = 255;
  static constexpr uint8_t REDUCED_BRIGHTNESS = 100;

  explicit PizzaDisplay(
      drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
          &sequencer_controller_ref,
      musin::timing::TempoHandler &tempo_handler_ref,
      musin::Logger &logger_ref);

  PizzaDisplay(const PizzaDisplay &) = delete;
  PizzaDisplay &operator=(const PizzaDisplay &) = delete;

  /**
   * @brief Initialize the LED driver and hardware.
   * Must be called once before using the display.
   * @return true if initialization was successful, false otherwise.
   */
  bool init();

  /**
   * @brief Updates the entire display by drawing all elements and sending to
   * hardware. This should be the primary method called from the main loop.
   * @param now The current absolute time, used for animations.
   */
  void update(absolute_time_t now);

  /**
   * @brief Set the global brightness level.
   * @param brightness Brightness level (0-255).
   */
  void set_brightness(uint8_t brightness);

  /**
   * @brief Set all LEDs to black. Does not call show().
   */
  void clear();

  /**
   * @brief Set a specific LED by its raw index. Does not call show().
   * @param index The 0-based index of the LED.
   * @param color The color to set.
   */
  void set_led(uint32_t index, Color color);

  /**
   * @brief Set the color of the Play button LED. Does not call show().
   * @param color The color to set.
   */
  void set_play_button_led(Color color);

  /**
   * @brief Set the color of a keypad LED based on intensity. Does not call
   * show().
   * @param row Row index (0-7).
   * @param col Column index (0-4). Note: Col 4 (sample select) might not have a
   * direct LED.
   * @param intensity Intensity value (0-127), used to scale a base color (e.g.,
   * white).
   */
  void set_keypad_led(uint8_t row, uint8_t col, uint8_t intensity);

  // get_note_color is removed, color is fetched via get_color_for_midi_note
  // using config::global_note_definitions

  /**
   * @brief Handles TempoEvent notifications for internal display logic (e.g.,
   * pulsing).
   */
  void notification(musin::timing::TempoEvent event);

  /**
   * @brief Handles NoteEvent notifications for triggering drumpad fades.
   */
  void notification(drum::Events::NoteEvent event);

  /**
   * @brief Handles SysExTransferStateChangeEvent notifications to show transfer
   * status.
   */
  void notification(drum::Events::SysExTransferStateChangeEvent event);

  /**
   * @brief Handles ParameterChangeEvent notifications to update visual effects.
   */
  void notification(drum::Events::ParameterChangeEvent event);

  // --- Drumpad Fade ---
  /**
   * @brief Initiates a fade effect on the specified drumpad LED.
   * @param pad_index The index of the drumpad (0-based).
   */
  void start_drumpad_fade(uint8_t pad_index);

  /**
   * @brief Clears (stops) the fade effect on the specified drumpad LED.
   * @param pad_index The index of the drumpad (0-based).
   */
  void clear_drumpad_fade(uint8_t pad_index);

  /**
   * @brief Gets the start time of the fade for a specific drumpad.
   * @param pad_index The index of the drumpad (0-based).
   * @return absolute_time_t The time the fade started, or nil_time if not
   * fading.
   */
  absolute_time_t get_drumpad_fade_start_time(uint8_t pad_index) const;

  /**
   * @brief Initiates the boot-up animation sequence.
   */
  void start_boot_animation();

  /**
   * @brief Switches the display to sequencer mode.
   * Public so modes can transition out (e.g., boot animation ends).
   */
  void switch_to_sequencer_mode();

private:
  void show();
  void update_highlight_state();

  /**
   * @brief Get the physical LED index corresponding to a sequencer track and
   * step.
   * @param track_idx The track index (0-based).
   * @param step_idx The step index (0-based).
   * @return std::optional<uint32_t> The physical LED index if valid, otherwise
   * std::nullopt.
   */
  std::optional<uint32_t> get_sequencer_led_index(size_t track_idx,
                                                  size_t step_idx) const;

  /**
   * @brief Get the physical LED index corresponding to a keypad row and column.
   * @param row The keypad row index (0-7).
   * @param col The keypad column index (0-3 for sequencer LEDs).
   * @return std::optional<uint32_t> The physical LED index if valid, otherwise
   * std::nullopt.
   */
  std::optional<uint32_t> get_keypad_led_index(uint8_t row, uint8_t col) const;

  /**
   * @brief Calculate a white color scaled by an intensity value.
   * @param intensity The intensity (0-127).
   * @return Color The calculated color.
   */
  Color calculate_intensity_color(uint8_t intensity) const;

  std::optional<Color> get_color_for_midi_note(uint8_t midi_note_number) const;
  void _set_physical_drumpad_led(uint8_t pad_index, Color color);

  musin::drivers::WS2812_DMA<NUM_LEDS> _leds;
  etl::array<absolute_time_t, config::NUM_DRUMPADS> _drumpad_fade_start_times;
  etl::array<std::optional<Color>, SEQUENCER_TRACKS_DISPLAYED>
      _track_override_colors;

  musin::Logger &_logger_ref;

  // State for modes to access
  std::atomic<uint32_t> _clock_tick_counter = 0;
  uint32_t _last_tick_count_for_highlight = 0;
  bool _highlight_is_bright = true;
  float _filter_value = 0.0f;
  float _crush_value = 0.0f;

  // Strategy pattern members
  ui::SequencerDisplayMode sequencer_mode_;
  ui::FileTransferDisplayMode transfer_mode_;
  ui::BootAnimationMode boot_animation_mode_;
  ui::DisplayMode *current_mode_ = nullptr;
};

inline std::optional<uint32_t>
PizzaDisplay::get_sequencer_led_index(size_t track_idx, size_t step_idx) const {
  // Map logical track index to physical column index (0->3, 1->2, 2->1, 3->0)
  size_t physical_col_idx = (SEQUENCER_TRACKS_DISPLAYED - 1) - track_idx;
  size_t led_array_index =
      step_idx * SEQUENCER_TRACKS_DISPLAYED + physical_col_idx;

  if (led_array_index < LED_ARRAY.size()) {
    return LED_ARRAY[led_array_index];
  }
  return std::nullopt;
}

inline std::optional<uint32_t>
PizzaDisplay::get_keypad_led_index(uint8_t row, uint8_t col) const {
  if (col >= SEQUENCER_TRACKS_DISPLAYED) {
    return std::nullopt;
  }
  if (row >= SEQUENCER_STEPS_DISPLAYED) {
    return std::nullopt;
  }
  uint8_t step_index = (SEQUENCER_STEPS_DISPLAYED - 1) - row;
  size_t array_index = step_index * SEQUENCER_TRACKS_DISPLAYED + col;

  if (array_index < LED_ARRAY.size()) {
    return LED_ARRAY[array_index];
  }
  return std::nullopt;
}

} // namespace drum

#endif // PIZZA_DISPLAY_H
