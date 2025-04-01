// Filename: keypad_hc138.h

#ifndef DRUM_DRIVERS_KEYPAD_HC138_H
#define DRUM_DRIVERS_KEYPAD_HC138_H

#include <cstdint>
#include <cstddef> // For size_t
#include <array>   // For fixed-size decoder pins array

// Wrap C SDK headers
extern "C" {
#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/assert.h" // For panic etc.
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
               std::uint32_t hold_time_us = DEFAULT_HOLD_TIME_US)
    : _num_rows(num_rows),
      _num_cols(num_cols),
      _decoder_address_pins(decoder_address_pins),
      _col_pins(col_pins),
      _key_data(key_data_buffer), // Store the pointer to the user's buffer
      _scan_interval_us(scan_interval_us),
      _debounce_time_us(debounce_time_us),
      _hold_time_us(hold_time_us),
      _last_scan_time(nil_time)
  {
    // --- Input Validation ---
    // Use panic for unrecoverable configuration errors as per guidelines
    if (_num_rows == 0 || _num_rows > 8) {
      panic("Keypad_HC138: Invalid number of rows (%u). Must be 1-8.", _num_rows);
    }
    if (_num_cols == 0) {
      panic("Keypad_HC138: Invalid number of columns (%u). Must be > 0.", _num_cols);
    }
    if (_col_pins == nullptr) {
        panic("Keypad_HC138: Column pins array pointer is null.");
    }
    if (_key_data == nullptr) {
      panic("Keypad_HC138: Key data buffer pointer is null.");
    }
    // We trust the user provided the correct buffer size, cannot check easily here without size param

     // Initialize the key data buffer provided by the user
     for (size_t i = 0; i < (size_t)_num_rows * _num_cols; ++i) {
         _key_data[i] = {}; // Default initialize KeyData structs
     }
  }

  // Prevent copying and assignment
  Keypad_HC138(const Keypad_HC138&) = delete;
  Keypad_HC138& operator=(const Keypad_HC138&) = delete;

  /**
   * @brief Initialize GPIO pins for the keypad.
   * Must be called once before starting scans.
   */
  void init() {
    // Initialize Decoder Address Pins (Outputs)
    for (uint pin : _decoder_address_pins) {
      gpio_init(pin);
      gpio_set_dir(pin, GPIO_OUT);
      gpio_put(pin, 0); // Start with address 0
    }

    // Initialize Column Pins (Inputs with Pull-ups)
    for (std::uint8_t c = 0; c < _num_cols; ++c) {
      uint pin = _col_pins[c];
      gpio_init(pin);
      gpio_set_dir(pin, GPIO_IN);
      gpio_pull_up(pin);
      // Optional: Enable hysteresis for potentially noisy inputs
      // gpio_set_input_hysteresis_enabled(pin, true);
    }

    _last_scan_time = get_absolute_time();
  }

  /**
   * @brief Performs a scan cycle if the scan interval has elapsed.
   * This function should be called periodically in the main loop or via a timer.
   * It updates the internal state of all keys based on raw input, debouncing, and hold timing.
   * @return true if a scan was performed, false otherwise.
   */
  bool scan() {
    absolute_time_t now = get_absolute_time();
    uint64_t diff_us = absolute_time_diff_us(_last_scan_time, now);

    if (diff_us < _scan_interval_us) {
      return false; // Not time to scan yet
    }
    _last_scan_time = now;

    // --- Clear transient flags before scan ---
    for (std::uint8_t r = 0; r < _num_rows; ++r) {
      for (std::uint8_t c = 0; c < _num_cols; ++c) {
        KeyData& key = _key_data[r * _num_cols + c];
        key.just_pressed = false;
        key.just_released = false;
      }
    }

    // --- Perform scan ---
    for (std::uint8_t r = 0; r < _num_rows; ++r) {
      select_row(r);

      // Small delay might be needed for GPIOs and decoder to settle.
      // Adjust timing based on hardware specifics. 1-5us is often sufficient.
      // Use SDK delay for precision if needed: busy_wait_us(2);
      // Or rely on the loop overhead if fast enough.

      for (std::uint8_t c = 0; c < _num_cols; ++c) {
        // Read raw state: LOW (false) means pressed (row LOW, col pulled HIGH)
        bool raw_key_pressed = !gpio_get(_col_pins[c]);

        // Update state machine for this key
        update_key_state(r, c, raw_key_pressed, now);
      }
    }

    // Optional: De-select row (set address lines low or to an unused state)
    select_row(0); // Or select an address > num_rows if needed

    return true; // Scan was performed
  }

  /**
   * @brief Checks if a specific key is currently considered pressed (includes held state).
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key state is PRESSED or HOLDING, false otherwise.
   */
  bool is_pressed(std::uint8_t row, std::uint8_t col) const {
    if (row >= _num_rows || col >= _num_cols) return false;
    const KeyState current_state = _key_data[row * _num_cols + col].state;
    return (current_state == KeyState::PRESSED || current_state == KeyState::HOLDING);
  }

  /**
   * @brief Checks if a specific key transitioned to the PRESSED state during the *last completed* scan cycle.
   * This flag is cleared at the beginning of the next scan.
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key was just pressed, false otherwise.
   */
  bool was_pressed(std::uint8_t row, std::uint8_t col) const {
    if (row >= _num_rows || col >= _num_cols) return false;
    return _key_data[row * _num_cols + col].just_pressed;
  }

  /**
   * @brief Checks if a specific key transitioned back to the IDLE state (was released) during the *last completed* scan cycle.
   * This flag is cleared at the beginning of the next scan.
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key was just released, false otherwise.
   */
  bool was_released(std::uint8_t row, std::uint8_t col) const {
     if (row >= _num_rows || col >= _num_cols) return false;
    return _key_data[row * _num_cols + col].just_released;
  }

  /**
   * @brief Checks if a specific key is currently in the HOLDING state.
   *
   * @param row Row index (0 to num_rows-1).
   * @param col Column index (0 to num_cols-1).
   * @return true if the key state is HOLDING, false otherwise.
   */
  bool is_held(std::uint8_t row, std::uint8_t col) const {
    if (row >= _num_rows || col >= _num_cols) return false;
    return (_key_data[row * _num_cols + col].state == KeyState::HOLDING);
  }

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
  void select_row(std::uint8_t row) {
    // Ensure row is within valid range for the decoder (0-7)
    // This check prevents issues even if _num_rows is smaller
    if (row >= 8) return;

    // Set A0, A1, A2 based on row number bits
    gpio_put(_decoder_address_pins[0], (row >> 0) & 1); // A0 = LSB
    gpio_put(_decoder_address_pins[1], (row >> 1) & 1); // A1
    gpio_put(_decoder_address_pins[2], (row >> 2) & 1); // A2 = MSB
  }

  /**
   * @brief Updates the state machine for a single key based on its raw state and timings.
   *
   * @param r Row index.
   * @param c Column index.
   * @param raw_key_pressed Current raw physical state (true if GPIO reads LOW).
   * @param now The current time.
   */
  void update_key_state(std::uint8_t r, std::uint8_t c, bool raw_key_pressed, absolute_time_t now) {
    // Get mutable reference to the key's data
    KeyData& key = _key_data[r * _num_cols + c];

    switch (key.state) {
      case KeyState::IDLE:
        if (raw_key_pressed) {
          // Potential press detected, start debounce timer
          key.state = KeyState::DEBOUNCING_PRESS;
          key.transition_time = now;
        }
        break;

      case KeyState::DEBOUNCING_PRESS:
        if (raw_key_pressed) {
          // Still pressed, check if debounce time has passed
          if (absolute_time_diff_us(key.transition_time, now) >= _debounce_time_us) {
            // Debounce confirmed, transition to PRESSED
            key.state = KeyState::PRESSED;
            key.transition_time = now; // Record press time for hold check
            key.just_pressed = true;   // Set event flag
            // Check immediately if hold time is zero or very small
             if (_hold_time_us == 0 || absolute_time_diff_us(key.transition_time, now) >= _hold_time_us) {
                 key.state = KeyState::HOLDING;
             }
          }
          // else: Debounce time not yet elapsed, remain in DEBOUNCING_PRESS
        } else {
          // Key released during debounce, return to IDLE
          key.state = KeyState::IDLE;
          key.transition_time = nil_time;
        }
        break;

      case KeyState::PRESSED:
        if (raw_key_pressed) {
          // Still pressed, check if hold time has passed
          if (absolute_time_diff_us(key.transition_time, now) >= _hold_time_us) {
            key.state = KeyState::HOLDING;
            // Note: transition_time remains the original press time
          }
          // else: Hold time not yet elapsed, remain in PRESSED
        } else {
          // Potential release detected, start debounce timer
          key.state = KeyState::DEBOUNCING_RELEASE;
          key.transition_time = now; // Record time release *started*
        }
        break;

      case KeyState::HOLDING:
        if (!raw_key_pressed) {
          // Potential release detected (from HELD state), start debounce timer
          key.state = KeyState::DEBOUNCING_RELEASE;
          key.transition_time = now; // Record time release *started*
        }
        // else: Still held, remain in HOLDING state
        break;

      case KeyState::DEBOUNCING_RELEASE:
        if (!raw_key_pressed) {
          // Still released, check if debounce time has passed
          if (absolute_time_diff_us(key.transition_time, now) >= _debounce_time_us) {
            // Debounce confirmed, transition to IDLE
            key.state = KeyState::IDLE;
            key.transition_time = nil_time;
            key.just_released = true; // Set event flag
          }
          // else: Debounce time not yet elapsed, remain in DEBOUNCING_RELEASE
        } else {
          // Key pressed again during release debounce
          // Decide whether to go back to PRESSED or HOLDING based on original press time
          // Going back to PRESSED is simpler and often sufficient.
          key.state = KeyState::PRESSED;
           // Re-check hold condition immediately in case it was bouncing near hold threshold
           if (absolute_time_diff_us(_key_data[r * _num_cols + c].transition_time, now) >= _hold_time_us) {
                 key.state = KeyState::HOLDING;
           }

          // Do NOT reset transition_time, keep the original press time for hold calculation.
          // Do NOT set just_pressed flag here.
        }
        break;
    } // end switch
  }

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