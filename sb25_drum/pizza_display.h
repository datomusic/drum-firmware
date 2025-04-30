#ifndef PIZZA_DISPLAY_H
#define PIZZA_DISPLAY_H

#include "drum_pizza_hardware.h" // For NUM_LEDS, LED_* constants, PIN_LED_DATA etc.
#include "etl/array.h"
#include "musin/drivers/ws2812.h"
#include <algorithm> // For std::min
#include <cstddef>   // For size_t
#include <cstdint>
#include <optional>  // For std::optional

// Include the actual sequencer header
#include "step_sequencer.h"

// Forward declaration for check_external_pin_state if needed, or include main.h if it's there
// Assuming check_external_pin_state remains accessible or is moved/duplicated.
// For now, let's assume it's available globally or we'll handle it in init.

namespace PizzaExample {

class PizzaDisplay {
public:
  // --- Constants ---
  // Note: NUM_LEDS is expected to be defined in drum_pizza_hardware.h
  static constexpr size_t SEQUENCER_TRACKS_DISPLAYED = 4;
  static constexpr size_t SEQUENCER_STEPS_DISPLAYED = 8;
  static constexpr size_t NUM_NOTE_COLORS = 32;
  static constexpr uint16_t VELOCITY_TO_BRIGHTNESS_SCALE = 2;
  static constexpr uint8_t HIGHLIGHT_BLEND_AMOUNT = 100;
  static constexpr uint32_t COLOR_WHITE = 0xFFFFFF;
  static constexpr uint16_t INTENSITY_TO_BRIGHTNESS_SCALE = 2;
  static constexpr uint8_t MAX_BRIGHTNESS = 255;

  // --- Public Methods ---
  PizzaDisplay();

  // Prevent copying and assignment
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
   * @return The physical LED index or NUM_LEDS if invalid.
   */
  uint32_t get_drumpad_led_index(uint8_t pad_index) const;

  /**
   * @brief Update the keypad LEDs to reflect the current state of the sequencer.
   * @tparam NumTracks Number of tracks in the sequencer.
   * @tparam NumSteps Number of steps per track in the sequencer.
   * @param sequencer A const reference to the sequencer object.
   * @param current_step The index of the currently playing step to highlight (0 to NumSteps-1).
   */
  template <size_t NumTracks, size_t NumSteps>
  void draw_sequencer_state(const StepSequencer::Sequencer<NumTracks, NumSteps> &sequencer,
                            uint32_t current_step); // Declaration only

  /**
   * @brief Get a const reference to the underlying WS2812 driver instance.
   * Allows access to driver methods like adjust_color_brightness.
   * @return const Musin::Drivers::WS2812<NUM_LEDS>&
   */
  const Musin::Drivers::WS2812<NUM_LEDS> &leds() const {
    return _leds;
  } // Return the renamed member

private:
  // --- Private Helper Methods ---

  /**
   * @brief Calculate the LED color for a sequencer step based on note and velocity.
   * @param step The sequencer step data.
   * @return uint32_t The calculated color (0xRRGGBB), or 0 if step is disabled/invalid.
   */
  uint32_t calculate_step_color(const StepSequencer::Step &step) const;

  /**
   * @brief Apply a highlight effect (blend with white) to a color.
   * @param color The base color.
   * @return uint32_t The highlighted color.
   */
  uint32_t apply_highlight(uint32_t color) const;

  /**
   * @brief Get the physical LED index corresponding to a sequencer track and step.
   * @param track_idx The track index (0-based).
   * @param step_idx The step index (0-based).
   * @return std::optional<uint32_t> The physical LED index if valid, otherwise std::nullopt.
   */
  std::optional<uint32_t> get_sequencer_led_index(size_t track_idx, size_t step_idx) const;

  // --- Private Members ---
  Musin::Drivers::WS2812<NUM_LEDS> _leds;
  etl::array<uint32_t, NUM_NOTE_COLORS> note_colors; // Use constant for size
};

// --- Template Function Definitions (must be in header) ---

template <size_t NumTracks, size_t NumSteps>
void PizzaDisplay::draw_sequencer_state(const StepSequencer::Sequencer<NumTracks, NumSteps> &sequencer,
                                        uint32_t current_step) {
  // Ensure current_step is within the valid range [0, NumSteps-1]
  uint32_t current_step_in_pattern = (NumSteps > 0) ? (current_step % NumSteps) : 0;

  for (size_t track_idx = 0; track_idx < NumTracks; ++track_idx) {
    // Only display tracks that map to keypad columns
    if (track_idx >= SEQUENCER_TRACKS_DISPLAYED)
      continue;

    const auto &track = sequencer.get_track(track_idx);
    for (size_t step_idx = 0; step_idx < NumSteps; ++step_idx) {
      // Only display steps that map to keypad rows
      if (step_idx >= SEQUENCER_STEPS_DISPLAYED)
        continue;

      const auto &step = track.get_step(step_idx);
      uint32_t final_color = calculate_step_color(step);

      // Highlight the current step
      if (step_idx == current_step_in_pattern) {
        final_color = apply_highlight(final_color);
      }

      // Get the physical LED index for this step
      std::optional<uint32_t> led_index_opt = get_sequencer_led_index(track_idx, step_idx);

      // Set the pixel if the index is valid
      if (led_index_opt.has_value()) {
        _leds.set_pixel(led_index_opt.value(), final_color);
      }
    }
  }
}

// --- Inline Helper Method Definitions ---

inline uint32_t PizzaDisplay::calculate_step_color(const StepSequencer::Step &step) const {
  uint32_t color = 0; // Default to black (off)

  if (step.enabled && step.note.has_value()) {
    // Use modulo with the defined number of colors
    uint32_t base_color = get_note_color(step.note.value() % NUM_NOTE_COLORS);

    // Determine brightness based on velocity
    uint8_t brightness = MAX_BRIGHTNESS; // Default to full brightness
    if (step.velocity.has_value()) {
      // Scale velocity (1-127) to brightness, clamp at MAX_BRIGHTNESS
      uint16_t calculated_brightness =
          static_cast<uint16_t>(step.velocity.value()) * VELOCITY_TO_BRIGHTNESS_SCALE;
      brightness = static_cast<uint8_t>(
          std::min(calculated_brightness, static_cast<uint16_t>(MAX_BRIGHTNESS)));
    }

    // Apply brightness using the WS2812 method
    color = _leds.adjust_color_brightness(base_color, brightness);
  }
  return color;
}

inline uint32_t PizzaDisplay::apply_highlight(uint32_t color) const {
  // Simple additive blend: Add white component
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  // Use std::min with explicit int type to avoid potential promotion issues before cast
  r = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(r) + HIGHLIGHT_BLEND_AMOUNT));
  g = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(g) + HIGHLIGHT_BLEND_AMOUNT));
  b = static_cast<uint8_t>(
      std::min<int>(MAX_BRIGHTNESS, static_cast<int>(b) + HIGHLIGHT_BLEND_AMOUNT));
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

inline std::optional<uint32_t> PizzaDisplay::get_sequencer_led_index(size_t track_idx,
                                                                     size_t step_idx) const {
  // Map step_idx/track_idx to the linear LED_ARRAY index
  size_t led_array_index =
      step_idx * SEQUENCER_TRACKS_DISPLAYED + static_cast<size_t>(track_idx);
  if (led_array_index < LED_ARRAY.size()) { // Bounds check against LED_ARRAY size
    return LED_ARRAY[led_array_index];
  }
  return std::nullopt;
}

} // namespace PizzaExample

#endif // PIZZA_DISPLAY_H
