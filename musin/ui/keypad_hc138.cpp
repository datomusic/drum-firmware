
#include "keypad_hc138.h"

// Wrap C SDK headers needed for implementation
extern "C" {
#include "hardware/gpio.h"
#include "pico/assert.h" // For panic etc.
#include "pico/time.h"   // For get_absolute_time, absolute_time_diff_us, nil_time
}

namespace drum_drivers {

// --- Constructor Implementation ---
Keypad_HC138::Keypad_HC138(std::uint8_t num_rows, std::uint8_t num_cols,
                           const std::array<uint, 3>& decoder_address_pins,
                           const uint* col_pins,
                           KeyData* key_data_buffer,
                           std::uint32_t scan_interval_us,
                           std::uint32_t debounce_time_us,
                           std::uint32_t hold_time_us)
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


// --- init() Implementation ---
void Keypad_HC138::init() {
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


// --- scan() Implementation ---
bool Keypad_HC138::scan() {
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


// --- is_pressed() Implementation ---
bool Keypad_HC138::is_pressed(std::uint8_t row, std::uint8_t col) const {
  if (row >= _num_rows || col >= _num_cols) return false;
  const KeyState current_state = _key_data[row * _num_cols + col].state;
  return (current_state == KeyState::PRESSED || current_state == KeyState::HOLDING);
}


// --- was_pressed() Implementation ---
bool Keypad_HC138::was_pressed(std::uint8_t row, std::uint8_t col) const {
  if (row >= _num_rows || col >= _num_cols) return false;
  return _key_data[row * _num_cols + col].just_pressed;
}


// --- was_released() Implementation ---
bool Keypad_HC138::was_released(std::uint8_t row, std::uint8_t col) const {
   if (row >= _num_rows || col >= _num_cols) return false;
  return _key_data[row * _num_cols + col].just_released;
}


// --- is_held() Implementation ---
bool Keypad_HC138::is_held(std::uint8_t row, std::uint8_t col) const {
  if (row >= _num_rows || col >= _num_cols) return false;
  return (_key_data[row * _num_cols + col].state == KeyState::HOLDING);
}


// --- select_row() Implementation ---
void Keypad_HC138::select_row(std::uint8_t row) {
  // Ensure row is within valid range for the decoder (0-7)
  // This check prevents issues even if _num_rows is smaller
  if (row >= 8) return;

  // Set A0, A1, A2 based on row number bits
  gpio_put(_decoder_address_pins[0], (row >> 0) & 1); // A0 = LSB
  gpio_put(_decoder_address_pins[1], (row >> 1) & 1); // A1
  gpio_put(_decoder_address_pins[2], (row >> 2) & 1); // A2 = MSB
}


// --- update_key_state() Implementation ---
void Keypad_HC138::update_key_state(std::uint8_t r, std::uint8_t c, bool raw_key_pressed, absolute_time_t now) {
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

// Note: Private member variables are defined in the header file.

} // namespace drum_drivers
