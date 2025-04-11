
// Implementation file for Keypad_HC138 template class
// Included by keypad_hc138.h

// Note: keypad_hc138.h should already be included before this file.
// We only need implementation-specific includes here.

// Wrap C SDK headers needed for implementation
extern "C" {
#include "hardware/gpio.h"
#include "pico/assert.h" // For panic etc.
#include "pico/time.h"   // For get_absolute_time, absolute_time_diff_us, nil_time
}

// Note: Definitions are already within Musin::UI namespace via keypad_hc138.h include

// --- Constructor Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
Keypad_HC138<NumRows, NumCols>::Keypad_HC138(
                           const std::array<uint, 3>& decoder_address_pins,
                           const std::array<uint, NumCols>& col_pins,
                           etl::span<KeyData> key_data_buffer,
                           std::uint32_t scan_interval_us,
                           std::uint32_t debounce_time_us,
                           std::uint32_t hold_time_us):
    // NumRows and NumCols are template parameters, no need to store them as members
    _decoder_address_pins(decoder_address_pins),
    _col_pins(col_pins), // Store reference to the array
    _key_data(key_data_buffer), // Store the span
    _scan_interval_us(scan_interval_us),
    _debounce_time_us(debounce_time_us),
    _hold_time_us(hold_time_us),
    _last_scan_time(nil_time)
{
  // --- Runtime Input Validation ---
  // static_asserts in the header handle dimension range checks.
  // We still need to check the provided span size at runtime.
  constexpr size_t expected_size = static_cast<size_t>(NumRows) * NumCols;
  if (_key_data.size() != expected_size) {
      panic("Keypad_HC138: Key data buffer span size (%d) does not match expected size (%d).",
            _key_data.size(), expected_size);
  }
  // No need to check _col_pins for null, it's a reference.

   // Initialize the key data buffer provided by the user via the span
   for (KeyData& key : _key_data) {
       key = {}; // Default initialize KeyData structs
   }
}


// --- init() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
void Keypad_HC138<NumRows, NumCols>::init() {
  // Initialize Decoder Address Pins (Outputs)
  for (uint pin : _decoder_address_pins) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0); // Start with address 0 // TODO: Use GpioPin abstraction here later
  }

  // Initialize Column Pins (Inputs with Pull-ups) - Iterate through the std::array
  for (uint pin : _col_pins) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    // Optional: Enable hysteresis for potentially noisy inputs
    // gpio_set_input_hysteresis_enabled(pin, true);
  }

  _last_scan_time = get_absolute_time();
}


// --- scan() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
bool Keypad_HC138<NumRows, NumCols>::scan() {
  absolute_time_t now = get_absolute_time();
  uint64_t diff_us = absolute_time_diff_us(_last_scan_time, now);

  if (diff_us < _scan_interval_us) {
    return false; // Not time to scan yet
  }
  _last_scan_time = now;

  // --- Clear transient flags before scan ---
  // Use the span for iteration
  for (KeyData& key : _key_data) {
      key.just_pressed = false;
      key.just_released = false;
  }

  // --- Perform scan ---
  for (std::uint8_t r = 0; r < NumRows; ++r) { // Use template parameter
    select_row(r);

    // Small delay might be needed for GPIOs and decoder to settle.
    // Adjust timing based on hardware specifics. 1-5us is often sufficient.
    // Use SDK delay for precision if needed: busy_wait_us(2);
    // Or rely on the loop overhead if fast enough.
    busy_wait_us(2);

    for (std::uint8_t c = 0; c < NumCols; ++c) { // Use template parameter
      // Read raw state: LOW (false) means pressed (row LOW, col pulled HIGH)
      bool raw_key_pressed = !gpio_get(_col_pins[c]); // Access std::array element

      // Update state machine for this key
      update_key_state(r, c, raw_key_pressed, now);
    }
  }

  // Optional: De-select row (set address lines low or to an unused state)
  select_row(0); // Or select an address > num_rows if needed

  return true; // Scan was performed
}


// --- is_pressed() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
bool Keypad_HC138<NumRows, NumCols>::is_pressed(std::uint8_t row, std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols) return false; // Use template parameters
  const KeyState current_state = _key_data[row * NumCols + col].state; // Use template parameter
  return (current_state == KeyState::PRESSED || current_state == KeyState::HOLDING);
}


// --- was_pressed() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
bool Keypad_HC138<NumRows, NumCols>::was_pressed(std::uint8_t row, std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols) return false; // Use template parameters
  return _key_data[row * NumCols + col].just_pressed; // Use template parameter
}


// --- was_released() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
bool Keypad_HC138<NumRows, NumCols>::was_released(std::uint8_t row, std::uint8_t col) const {
   if (row >= NumRows || col >= NumCols) return false; // Use template parameters
  return _key_data[row * NumCols + col].just_released; // Use template parameter
}


// --- is_held() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
bool Keypad_HC138<NumRows, NumCols>::is_held(std::uint8_t row, std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols) return false; // Use template parameters
  return (_key_data[row * NumCols + col].state == KeyState::HOLDING); // Use template parameter
}


// --- select_row() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
void Keypad_HC138<NumRows, NumCols>::select_row(std::uint8_t row) {
  // Ensure row is within valid range for the decoder (0-7)
  // This check prevents issues even if _num_rows is smaller
  if (row >= 8) return;

  // Set A0, A1, A2 based on row number bits
  // TODO: Use GpioPin abstraction here later
  gpio_put(_decoder_address_pins[0], (row >> 0) & 1); // A0 = LSB
  gpio_put(_decoder_address_pins[1], (row >> 1) & 1); // A1
  gpio_put(_decoder_address_pins[2], (row >> 2) & 1); // A2 = MSB
}


// --- update_key_state() Implementation ---
template<std::uint8_t NumRows, std::uint8_t NumCols>
void Keypad_HC138<NumRows, NumCols>::update_key_state(std::uint8_t r, std::uint8_t c, bool raw_key_pressed, absolute_time_t now) {
  // Get mutable reference to the key's data using the span and template parameters
  KeyData& key = _key_data[r * NumCols + c];

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
         // Need to access the original transition time stored in the key data itself
         if (absolute_time_diff_us(key.transition_time, now) >= _hold_time_us) {
               key.state = KeyState::HOLDING;
         }

        // Do NOT reset transition_time, keep the original press time for hold calculation.
        // Do NOT set just_pressed flag here.
      }
      break;
  } // end switch
}

// Note: Private member variables are defined in the header file.
