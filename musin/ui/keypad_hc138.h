
#ifndef DRUM_DRIVERS_KEYPAD_HC138_H
#define DRUM_DRIVERS_KEYPAD_HC138_H

#include "etl/array.h"
#include "etl/observer.h"   // Include ETL observer pattern
#include "etl/span.h"       // Include ETL span
#include "etl/vector.h"     // Include ETL vector for storing GpioPin objects
#include "musin/hal/gpio.h" // Include the GPIO abstraction
#include "musin/hal/logger.h"
#include <cstddef>
#include <cstdint>

// Wrap C SDK headers
extern "C" {
#include "pico/time.h"
}

namespace musin::ui {

/**
 * @brief Event data structure for keypad notifications
 */
struct KeypadEvent {
  enum class Type : uint8_t {
    Press,
    Release,
    Hold,
    Tap
  };

  uint8_t row;
  uint8_t col;
  Type type;
};

// Forward declaration
template <std::uint8_t NumRows, std::uint8_t NumCols> class Keypad_HC138;

/**
 * @brief Represents the possible states of a single key.
 */
enum class KeyState : std::uint8_t {
  STUCK,             ///< Initial state or key is stuck pressed.
  IDLE,              ///< Key is up and stable.
  DEBOUNCING_PRESS,  ///< Key is potentially pressed, waiting for debounce
                     ///< confirmation.
  PRESSED,           ///< Key is confirmed pressed, hold timer running.
  HOLDING,           ///< Key is confirmed pressed and hold time has elapsed.
  DEBOUNCING_RELEASE ///< Key is potentially released, waiting for debounce
                     ///< confirmation.
};

/**
 * @brief Internal data structure to hold the state for a single key.
 */
struct KeyData {
  KeyState state = KeyState::STUCK; ///< Current debounced and hold state.
  absolute_time_t press_start_time =
      nil_time; ///< Time of the initial press for hold detection.
  absolute_time_t state_change_time =
      nil_time; ///< Time of the last state change for debouncing.
  absolute_time_t press_event_time =
      nil_time; ///< Time of the confirmed press event for tap detection.
  bool just_pressed = false;  ///< Flag indicating a transition to PRESSED
                              ///< occurred in the last scan.
  bool just_released = false; ///< Flag indicating a transition to IDLE occurred
                              ///< in the last scan.
};

/**
 * @brief Driver for a matrix keypad scanned using a 74HC138 decoder for rows.
 *
 * This driver handles debouncing and detects press, release, and hold events.
 * It requires the user to provide the memory buffer (via etl::span) for storing
 * key states to avoid dynamic allocation within the driver.
 *
 * @tparam NumRows Number of rows (1-8).
 * @tparam NumCols Number of columns (>0).
 * @tparam MaxObservers Maximum number of observers that can be attached.
 */
template <std::uint8_t NumRows, std::uint8_t NumCols>
class Keypad_HC138 : public etl::observable<etl::observer<KeypadEvent>, 4> {
public:
  // --- Compile-time validation ---
  static_assert(NumRows > 0 && NumRows <= 8,
                "Keypad_HC138: NumRows must be between 1 and 8.");
  static_assert(NumCols > 0, "Keypad_HC138: NumCols must be greater than 0.");

  // --- Constants ---
  static constexpr std::uint32_t DEFAULT_SCAN_INTERVAL_MS =
      10; ///< 10ms default scan rate
  static constexpr std::uint32_t DEFAULT_DEBOUNCE_TIME_MS =
      5; ///< 5ms default debounce time
  static constexpr std::uint32_t DEFAULT_HOLD_TIME_MS =
      500; ///< 500ms default hold time
  static constexpr std::uint32_t DEFAULT_TAP_TIME_MS =
      60; ///< 150ms default tap time threshold

  /**
   * @brief Construct a new Keypad_HC138 driver instance.
   *
   * @param decoder_address_pins Array containing the 3 GPIO pin numbers for
   * HC138 A0, A1, A2.
   * @param col_pins Array containing the GPIO pin numbers for the columns. Must
   * contain `NumCols` elements.
   * @param logger Reference to logger for diagnostic output.
   * @param scan_interval_ms Time between full keypad scans in milliseconds.
   * @param debounce_time_ms Time duration for debouncing transitions in
   * milliseconds.
   * @param hold_time_ms Minimum time a key must be pressed to be considered
   * 'held'.
   * @param tap_time_ms Maximum time between a press and release to be
   * considered a 'tap'.
   */
  Keypad_HC138(const etl::array<uint32_t, 3> &decoder_address_pins,
               const etl::array<uint32_t, NumCols> &col_pins,
               musin::Logger &logger,
               std::uint32_t scan_interval_ms = DEFAULT_SCAN_INTERVAL_MS,
               std::uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_TIME_MS,
               std::uint32_t hold_time_ms = DEFAULT_HOLD_TIME_MS,
               std::uint32_t tap_time_ms = DEFAULT_TAP_TIME_MS);

  // Prevent copying and assignment
  Keypad_HC138(const Keypad_HC138 &) = delete;
  Keypad_HC138 &operator=(const Keypad_HC138 &) = delete;

  /**
   * @brief Initialize GPIO pins for the keypad.
   * Must be called once before starting scans.
   */
  void init();

  /**
   * @brief Performs a scan cycle if the scan interval has elapsed.
   * This function should be called periodically in the main loop or via a
   * timer. It updates the internal state of all keys based on raw input,
   * debouncing, and hold timing.
   * @return true if a scan was performed, false otherwise.
   */
  bool scan();

  /**
   * @brief Checks if a specific key is currently considered pressed (includes
   * held state).
   *
   * @param row Row index (0 to NumRows-1).
   * @param col Column index (0 to NumCols-1).
   * @return true if the key state is PRESSED or HOLDING, false otherwise.
   */
  bool is_pressed(std::uint8_t row, std::uint8_t col) const;

  /**
   * @brief Checks if a specific key transitioned to the PRESSED state during
   * the *last completed* scan cycle. This flag is cleared at the beginning of
   * the next scan.
   *
   * @param row Row index (0 to NumRows-1).
   * @param col Column index (0 to NumCols-1).
   * @return true if the key was just pressed, false otherwise.
   */
  bool was_pressed(std::uint8_t row, std::uint8_t col) const;

  /**
   * @brief Checks if a specific key transitioned back to the IDLE state (was
   * released) during the *last completed* scan cycle. This flag is cleared at
   * the beginning of the next scan.
   *
   * @param row Row index (0 to NumRows-1).
   * @param col Column index (0 to NumCols-1).
   * @return true if the key was just released, false otherwise.
   */
  bool was_released(std::uint8_t row, std::uint8_t col) const;

  /**
   * @brief Checks if a specific key is currently in the HOLDING state.
   *
   * @param row Row index (0 to NumRows-1).
   * @param col Column index (0 to NumCols-1).
   * @return true if the key state is HOLDING, false otherwise.
   */
  bool is_held(std::uint8_t row, std::uint8_t col) const;

  /** @brief Get the configured number of rows (compile-time). */
  constexpr std::uint8_t get_num_rows() const {
    return NumRows;
  }

  /** @brief Get the configured number of columns (compile-time). */
  constexpr std::uint8_t get_num_cols() const {
    return NumCols;
  }

private:
  /**
   * @brief Sets the decoder address pins (A0, A1, A2) to select a specific row
   * output (Y0-Y7). The selected Y output goes LOW.
   * @param row The row number (0-7).
   */
  void select_row(std::uint8_t row);

  /**
   * @brief Updates the state machine for a single key based on its raw state
   * and timings.
   *
   * @param r Row index.
   * @param c Column index.
   * @param raw_key_pressed Current raw physical state (true if GPIO reads LOW).
   * @param now The current time.
   */
  void update_key_state(std::uint8_t r, std::uint8_t c, bool raw_key_pressed,
                        absolute_time_t now);

  // --- Configuration (initialized in constructor) ---
  // NumRows and NumCols are now template parameters
  // Store GpioPin objects directly using fixed-size vectors
  etl::vector<musin::hal::GpioPin, 3> _decoder_address_pins;
  etl::vector<musin::hal::GpioPin, NumCols> _col_pins;
  const std::uint32_t _scan_interval_us;
  const std::uint32_t _debounce_time_us;
  const std::uint32_t _hold_time_us;
  const std::uint32_t _tap_time_us;

  // --- State ---
  etl::array<KeyData, NumRows * NumCols> _internal_key_data; // Internal buffer
  absolute_time_t _last_scan_time = nil_time; // Initialize to nil_time
  musin::Logger &_logger;
  bool _first_scan_complete = false;

  // --- Private Notification Helper ---
  void notify_event(uint8_t r, uint8_t c, KeypadEvent::Type type) {
    this->notify_observers(KeypadEvent{r, c, type});
  }

}; // class Keypad_HC138

// Include the implementation file for the template class
#include "keypad_hc138.tpp"

} // namespace musin::ui
#endif // DRUM_DRIVERS_KEYPAD_HC138_H
