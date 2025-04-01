
#ifndef DRUM_DRIVERS_KEYPAD_HC138_H
#define DRUM_DRIVERS_KEYPAD_HC138_H

#include <cstdint>
#include <cstddef> // For size_t
#include <array>   // For fixed-size decoder pins array

// Wrap C SDK headers
extern "C" {
#include "pico/time.h" // For absolute_time_t, nil_time
// Note: hardware/gpio.h is NOT included here, only needed in .cpp
}

namespace drum_drivers {

/**
 * @brief Represents the possible states of a single key.
 */
enum class KeyState : std::uint8_t {
  IDLE,              ///< Key is up and stable.
  DEBOUNCING_PRESS,  ///< Key is potentially pressed, waiting for debounce confirmation.
  PRESSED,           ///< Key is confirmed pressed, hold timer running.
  HOLDING,           ///< Key is confirmed pressed and hold time has elapsed.
  DEBOUNCING_RELEASE ///< Key is potentially released, waiting for debounce confirmation.
};

/**
 * @brief Internal data structure to hold the state for a single key.
 */
struct KeyData {
  KeyState state = KeyState::IDLE;         ///< Current debounced and hold state.
  absolute_time_t transition_time = nil_time; ///< Time of the last relevant state change start.
  bool just_pressed = false;              ///< Flag indicating a transition to PRESSED occurred in the last scan.
  bool just_released = false;             ///< Flag indicating a transition to IDLE occurred in the last scan.
};

/**
 * @brief Driver for a matrix keypad scanned using a 74HC138 decoder for rows.
 *
 * This driver handles debouncing and detects press, release, and hold events.
 * It requires the user to provide the memory buffer for storing key states
 * to avoid dynamic allocation within the driver.
 */
class Keypad_HC138 {
public:
  // --- Constants ---
  static constexpr std::uint32_t DEFAULT_SCAN_INTERVAL_US = 10000; ///< 10ms default scan rate
  static constexpr std::uint32_t DEFAULT_DEBOUNCE_TIME_US = 5000;  ///< 5ms default debounce time
  static constexpr std::uint32_t DEFAULT_HOLD_TIME_US = 500000;   ///< 500ms default hold time

  /**
   * @brief Construct a new Keypad_HC138 driver instance.
   *
   * @param num_rows Number of rows used (1 to 8, connected via HC138 Y0-Y7).
   * @param num_cols Number of columns (connected directly to GPIOs).
   * @param decoder_address_pins Array containing the 3 GPIO pin numbers for HC138 A0, A1, A2.
   * @param col_pins Pointer to an array containing the GPIO pin numbers for the columns. Must contain `num_cols` elements.
   * @param key_data_buffer Pointer to a buffer allocated by the caller to store the state for each key. Must contain `num_rows * num_cols` elements of `KeyData`.
   * @param scan_interval_us Time between full keypad scans in microseconds.
   * @param debounce_time_us Time duration for debouncing transitions in microseconds.
   * @param hold_time_us Minimum time a key must be pressed to be considered 'held'.
   */
  Keypad_HC138(std::uint8_t num_rows, std::uint8_t num_cols,
               const std::array<uint, 3>& decoder_address_pins,
               const uint* col_pins,           // Pointer to column pin array
               KeyData* key_data_buffer,       // Pointer to user-provided state buffer
               std::uint32_t scan_interval_us = DEFAULT_SCAN_INTERVAL_US,
               std::uint32_t debounce_time_us = DEFAULT_DEBOUNCE_TIME_US,
               std::uint32_t hold_time_us = DEFAULT_HOLD_TIME_US);

  // Prevent copying and assignment
  Keypad_HC138(const Keypad_HC138&) = delete;
  Keypad_HC138& operator=(const Keypad_HC138&) = delete;

  /**
   * @brief Initialize GPIO pins for the keypad.
   * Must be called once before starting scans.
   */
  void init();

  /**
   * @brief Performs a scan cycle if the scan interval has elapsed.
   * This function should be called periodically in the main loop or via a timer.
   * It updates the internal state of all keys based on raw input, debouncing, and hold timing.
   * @return true if a scan was performed, false otherwise.
   */
  bool scan();

  /**
   * @brief Checks if a specific key is currently considered pressed (includes held state).
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key state is PRESSED or HOLDING, false otherwise.
   */
  bool is_pressed(std::uint8_t row, std::uint8_t col) const;

  /**
   * @brief Checks if a specific key transitioned to the PRESSED state during the *last completed* scan cycle.
   * This flag is cleared at the beginning of the next scan.
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key was just pressed, false otherwise.
   */
  bool was_pressed(std::uint8_t row, std::uint8_t col) const;

  /**
   * @brief Checks if a specific key transitioned back to the IDLE state (was released) during the *last completed* scan cycle.
   * This flag is cleared at the beginning of the next scan.
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key was just released, false otherwise.
   */
  bool was_released(std::uint8_t row, std::uint8_t col) const;

  /**
   * @brief Checks if a specific key is currently in the HOLDING state.
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key state is HOLDING, false otherwise.
   */
  bool is_held(std::uint8_t row, std::uint8_t col) const;

  /** @brief Get the configured number of rows. */
  std::uint8_t get_num_rows() const { return _num_rows; }

  /** @brief Get the configured number of columns. */
  std::uint8_t get_num_cols() const { return _num_cols; }

private:
  /**
   * @brief Sets the decoder address pins (A0, A1, A2) to select a specific row output (Y0-Y7).
   * The selected Y output goes LOW.
   * @param row The row number (0-7).
   */
  void select_row(std::uint8_t row);

  /**
   * @brief Updates the state machine for a single key based on its raw state and timings.
   *
   * @param r Row index.
   * @param c Column index.
   * @param raw_key_pressed Current raw physical state (true if GPIO reads LOW).
   * @param now The current time.
   */
  void update_key_state(std::uint8_t r, std::uint8_t c, bool raw_key_pressed, absolute_time_t now);

  // --- Configuration (initialized in constructor) ---
  const std::uint8_t _num_rows;
  const std::uint8_t _num_cols;
  const std::array<uint, 3> _decoder_address_pins; // Fixed size array for 3 address pins
  const uint* _col_pins;                           // Pointer to user's column pin array
  const std::uint32_t _scan_interval_us;
  const std::uint32_t _debounce_time_us;
  const std::uint32_t _hold_time_us;

  // --- State ---
  KeyData* _key_data;                  // Pointer to user-provided buffer for key states
  absolute_time_t _last_scan_time;

}; // class Keypad_HC138

} // namespace drum_drivers

#endif // DRUM_DRIVERS_KEYPAD_HC138_H
