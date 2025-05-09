#ifndef PIZZA_DISPLAY_H
#define PIZZA_DISPLAY_H

#include "drum_pizza_hardware.h"
#include "etl/array.h"
#include "musin/drivers/ws2812.h"
#include "pico/time.h" // For absolute_time_t
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "config.h" // For config::NUM_DRUMPADS
#include "musin/timing/step_sequencer.h"
#include "sequencer_controller.h"
#include "musin/timing/tempo_handler.h"  // For TempoHandler
#include "musin/timing/tempo_event.h"    // For TempoEvent
#include "etl/observer.h"              // For etl::observer

namespace drum {

class PizzaDisplay : public etl::observer<musin::timing::TempoEvent> {
public:
  static constexpr size_t SEQUENCER_TRACKS_DISPLAYED = 4;
  static constexpr size_t SEQUENCER_STEPS_DISPLAYED = 8;
  static constexpr size_t NUM_NOTE_COLORS = 32;
  static constexpr float MIN_FADE_BRIGHTNESS_FACTOR = 0.1f;
  static constexpr uint32_t FADE_DURATION_MS = 150;
  static constexpr uint16_t VELOCITY_TO_BRIGHTNESS_SCALE = 2;
  static constexpr uint8_t HIGHLIGHT_BLEND_AMOUNT = 100;
  static constexpr uint32_t COLOR_WHITE = 0xFFFFFF;
  static constexpr uint16_t INTENSITY_TO_BRIGHTNESS_SCALE = 2;
  static constexpr uint8_t MAX_BRIGHTNESS = 255;

  explicit PizzaDisplay(
      musin::timing::Sequencer<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &sequencer_ref,
      drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
          &sequencer_controller_ref,
      musin::timing::TempoHandler &tempo_handler_ref);

  PizzaDisplay(const PizzaDisplay &) = delete;
  PizzaDisplay &operator=(const PizzaDisplay &) = delete;

  /**
   * @brief Initialize the LED driver and hardware.
   * Must be called once before using the display.
   * @return true if initialization was successful, false otherwise.
   */
  bool init();

  /**
   * @brief Send the current LED buffer data to the physical strip.
   */
  void show();

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
   * @param color The 24-bit color (e.g., 0xRRGGBB).
   */
  void set_led(uint32_t index, uint32_t color);

  /**
   * @brief Set the color of the Play button LED. Does not call show().
   * @param color The 24-bit color.
   */
  void set_play_button_led(uint32_t color);

  /**
   * @brief Set the color of a keypad LED based on intensity. Does not call show().
   * @param row Row index (0-7).
   * @param col Column index (0-4). Note: Col 4 (sample select) might not have a direct LED.
   * @param intensity Intensity value (0-127), used to scale a base color (e.g., white).
   */
  void set_keypad_led(uint8_t row, uint8_t col, uint8_t intensity);

  /**
   * @brief Get the base color associated with a note index.
   * @param note_index Index of the note color (0-31).
   * @return The 24-bit base color (0xRRGGBB) or 0 if index is invalid.
   */
  uint32_t get_note_color(uint8_t note_index) const;

  /**
   * @brief Get the physical LED index for a given drumpad.
   * @param pad_index Index of the drumpad (0-3).
   * @return std::optional containing the physical LED index if valid, otherwise std::nullopt.
   */

  /**
   * @brief Updates core LED elements like the play button and sequencer steps.
   * This method should be called regularly in the main loop before show().
   */
  void update_core_leds();

  /**
   * @brief Handles TempoEvent notifications for internal display logic (e.g., pulsing).
   */
  void notification(musin::timing::TempoEvent event) override;

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
   * @return absolute_time_t The time the fade started, or nil_time if not fading.
   */
  absolute_time_t get_drumpad_fade_start_time(uint8_t pad_index) const;

  /**
   * @brief Updates all drumpad LEDs based on their current base colors and fade states.
   * This should be called once per update cycle after base colors and fade triggers are set.
   * @param now The current absolute time.
   */
  void refresh_drumpad_leds(absolute_time_t now);

  /**
   * @brief Update the keypad LEDs to reflect the current state of the sequencer.
   * @tparam NumTracks Number of tracks in the sequencer.
   * @tparam NumSteps Number of steps per track in the sequencer.
   * @param sequencer A const reference to the sequencer data object.
   * @param controller A const reference to the sequencer controller object.
   */
  template <size_t NumTracks, size_t NumSteps>
  void draw_sequencer_state(const musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
                            const drum::SequencerController<NumTracks, NumSteps> &controller);

  /**
   * @brief Get a const reference to the underlying WS2812 driver instance.
   * Allows access to driver methods like adjust_color_brightness.
   * @return const musin::drivers::WS2812<NUM_LEDS>&
   */
  const musin::drivers::WS2812<NUM_LEDS> &leds() const {
    return _leds;
  }

private:
  /**
   * @brief Calculate the LED color for a sequencer step based on note and velocity.
   * @param step The sequencer step data.
   * @return uint32_t The calculated color (0xRRGGBB), or 0 if step is disabled/invalid.
   */
  uint32_t calculate_step_color(const musin::timing::Step &step) const;

  /**
   * @brief Apply a highlight effect (blend with white) to a color.
   * @param color The base color.
   * @return uint32_t The highlighted color (fixed blend).
   */
  uint32_t apply_highlight(uint32_t color) const;

  /**
   * @brief Apply a fading highlight effect (blend with white) based on a factor.
   * @param color The base color.
   * @param highlight_factor The intensity of the highlight (0.0 = none, 1.0 = full white blend).
   * @return uint32_t The highlighted color.
   */
  uint32_t apply_fading_highlight(uint32_t color, float highlight_factor) const;

  /**
   * @brief Get the physical LED index corresponding to a sequencer track and step.
   * @param track_idx The track index (0-based).
   * @param step_idx The step index (0-based).
   * @return std::optional<uint32_t> The physical LED index if valid, otherwise std::nullopt.
   */
  std::optional<uint32_t> get_sequencer_led_index(size_t track_idx, size_t step_idx) const;

  /**
   * @brief Get the physical LED index corresponding to a keypad row and column.
   * @param row The keypad row index (0-7).
   * @param col The keypad column index (0-3 for sequencer LEDs).
   * @return std::optional<uint32_t> The physical LED index if valid, otherwise std::nullopt.
   */
  std::optional<uint32_t> get_keypad_led_index(uint8_t row, uint8_t col) const;

  /**
   * @brief Calculate a white color scaled by an intensity value.
   * @param intensity The intensity (0-127).
   * @return uint32_t The calculated color (0xRRGGBB).
   */
  uint32_t calculate_intensity_color(uint8_t intensity) const;

  musin::drivers::WS2812<NUM_LEDS> _leds;
  etl::array<uint32_t, NUM_NOTE_COLORS> note_colors;
  etl::array<absolute_time_t, config::NUM_DRUMPADS> _drumpad_fade_start_times;

  // Model References
  musin::timing::Sequencer<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK> &_sequencer_ref;
  drum::SequencerController<config::NUM_TRACKS, config::NUM_STEPS_PER_TRACK>
      &_sequencer_controller_ref;
  musin::timing::TempoHandler &_tempo_handler_ref;

  uint32_t _clock_tick_counter = 0;
  float _stopped_highlight_factor = 0.0f;

  void _set_physical_drumpad_led(uint8_t pad_index, uint32_t color);
};

// --- Template Implementation ---

template <size_t NumTracks, size_t NumSteps>
void PizzaDisplay::draw_sequencer_state(
    const musin::timing::Sequencer<NumTracks, NumSteps> &sequencer,
    const drum::SequencerController<NumTracks, NumSteps> &controller) {

  bool is_running = _sequencer_controller_ref.is_running();
  // _stopped_highlight_factor is now a member and updated in update_core_leds()

  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    if (track_idx >= SEQUENCER_TRACKS_DISPLAYED)
      continue;

    const auto &track_data = sequencer.get_track(track_idx);

    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      if (step_idx >= SEQUENCER_STEPS_DISPLAYED)
        continue;

      const auto &step = track_data.get_step(step_idx);
      uint32_t final_color = calculate_step_color(step);

      std::optional<size_t> just_played_step = controller.get_last_played_step_for_track(track_idx);
      if (just_played_step.has_value() && step_idx == just_played_step.value()) {
        if (is_running) {
          final_color = apply_highlight(final_color);
        } else {
          final_color = apply_fading_highlight(final_color, _stopped_highlight_factor);
        }
      }

      std::optional<uint32_t> led_index_opt = get_sequencer_led_index(track_idx, step_idx);

      if (led_index_opt.has_value()) {
        _leds.set_pixel(led_index_opt.value(), final_color);
      }
    }
  }
}

inline uint32_t PizzaDisplay::calculate_step_color(const musin::timing::Step &step) const {
  uint32_t color = 0;

  if (step.enabled && step.note.has_value()) {
    uint32_t base_color = get_note_color(step.note.value() % NUM_NOTE_COLORS);

    uint8_t brightness = MAX_BRIGHTNESS;
    if (step.velocity.has_value()) {
      uint16_t calculated_brightness =
          static_cast<uint16_t>(step.velocity.value()) * VELOCITY_TO_BRIGHTNESS_SCALE;
      brightness = static_cast<uint8_t>(
          std::min(calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
    }

    color = _leds.adjust_color_brightness(base_color, brightness);
  }
  return color;
}

// Kept original implementation: Fixed highlight blend
inline uint32_t PizzaDisplay::apply_highlight(uint32_t color) const {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  r = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(r) + HIGHLIGHT_BLEND_AMOUNT));
  g = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(g) + HIGHLIGHT_BLEND_AMOUNT));
  b = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(b) + HIGHLIGHT_BLEND_AMOUNT));
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

// New implementation: Fading highlight blend based on factor
inline uint32_t PizzaDisplay::apply_fading_highlight(uint32_t color, float highlight_factor) const {
  uint8_t base_r = (color >> 16) & 0xFF;
  uint8_t base_g = (color >> 8) & 0xFF;
  uint8_t base_b = color & 0xFF;

  // Target highlight color is white
  constexpr uint8_t highlight_r = 0xFF;
  constexpr uint8_t highlight_g = 0xFF;
  constexpr uint8_t highlight_b = 0xFF;

  // Scale factor for integer blending (0-255)
  uint8_t blend_amount = static_cast<uint8_t>(std::clamp(highlight_factor * 255.0f, 0.0f, 255.0f));

  // Linear interpolation using integer math (lerp)
  uint8_t final_r = static_cast<uint8_t>((static_cast<uint32_t>(base_r) * (255 - blend_amount) +
                                          static_cast<uint32_t>(highlight_r) * blend_amount) /
                                         255);
  uint8_t final_g = static_cast<uint8_t>((static_cast<uint32_t>(base_g) * (255 - blend_amount) +
                                          static_cast<uint32_t>(highlight_g) * blend_amount) /
                                         255);
  uint8_t final_b = static_cast<uint8_t>((static_cast<uint32_t>(base_b) * (255 - blend_amount) +
                                          static_cast<uint32_t>(highlight_b) * blend_amount) /
                                         255);

  return (static_cast<uint32_t>(final_r) << 16) | (static_cast<uint32_t>(final_g) << 8) | final_b;
}

inline std::optional<uint32_t> PizzaDisplay::get_sequencer_led_index(size_t track_idx,
                                                                     size_t step_idx) const {
  // Map logical track index to physical column index (0->3, 1->2, 2->1, 3->0)
  size_t physical_col_idx = (SEQUENCER_TRACKS_DISPLAYED - 1) - track_idx;
  size_t led_array_index = step_idx * SEQUENCER_TRACKS_DISPLAYED + physical_col_idx;

  if (led_array_index < LED_ARRAY.size()) {
    return LED_ARRAY[led_array_index];
  }
  return std::nullopt;
}

inline std::optional<uint32_t> PizzaDisplay::get_keypad_led_index(uint8_t row, uint8_t col) const {
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

inline uint32_t PizzaDisplay::calculate_intensity_color(uint8_t intensity) const {
  uint16_t calculated_brightness = static_cast<uint16_t>(intensity) * INTENSITY_TO_BRIGHTNESS_SCALE;
  uint8_t brightness_val =
      static_cast<uint8_t>(std::min(calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
  return _leds.adjust_color_brightness(COLOR_WHITE, brightness_val);
}

} // namespace drum

#endif // PIZZA_DISPLAY_H
