#ifndef PIZZA_DISPLAY_H
#define PIZZA_DISPLAY_H

#include "drum_pizza_hardware.h" // For NUM_LEDS, LED_* constants, PIN_LED_DATA etc.
#include "etl/array.h"
#include "musin/drivers/ws2812.h"
#include <cstddef> // For size_t
#include <cstdint>

// Include the actual sequencer header
#include "step_sequencer.h"

// Forward declaration for check_external_pin_state if needed, or include main.h if it's there
// Assuming check_external_pin_state remains accessible or is moved/duplicated.
// For now, let's assume it's available globally or we'll handle it in init.

namespace PizzaExample {

class PizzaDisplay {
public:
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
   */
  template <size_t NumTracks, size_t NumSteps>
  void draw_sequencer_state(const StepSequencer::Sequencer<NumTracks, NumSteps> &sequencer);
    
  /**
   * @brief Get a const reference to the underlying WS2812 driver instance.
   * Allows access to driver methods like adjust_color_brightness.
   * @return const Musin::Drivers::WS2812<NUM_LEDS>&
   */
  const Musin::Drivers::WS2812<NUM_LEDS>& leds() const { return _leds; } // Return the renamed member
        
private:
  Musin::Drivers::WS2812<NUM_LEDS> _leds; // Renamed member variable
  etl::array<uint32_t, 32> note_colors;
};

} // namespace PizzaExample

#endif // PIZZA_DISPLAY_H
