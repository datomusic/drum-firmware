// Implementation file for Keypad_HC138 template class
// Included by keypad_hc138.h

// Note: keypad_hc138.h should already be included before this file.
// We only need implementation-specific includes here.

// Wrap C SDK headers needed for implementation
// hardware/gpio.h is no longer needed directly
extern "C" {
#include "pico/assert.h" // For panic etc.
#include "pico/time.h"   // For get_absolute_time, absolute_time_diff_us, nil_time
}

// Note: Definitions are already within Musin::UI namespace via keypad_hc138.h include

// --- Constructor Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
Keypad_HC138<NumRows, NumCols, Observers...>::Keypad_HC138(
    const etl::array<uint32_t, 3> &decoder_address_pins,
    const etl::array<uint32_t, NumCols> &col_pins,
    // No key_data_buffer parameter
    std::uint32_t scan_interval_ms, std::uint32_t debounce_time_ms, std::uint32_t hold_time_ms,
    std::uint32_t tap_time_ms)
    : // Store timing parameters (convert ms to us for internal storage)
      _scan_interval_us(scan_interval_ms * 1000), _debounce_time_us(debounce_time_ms * 1000),
      _hold_time_us(hold_time_ms * 1000), _tap_time_us(tap_time_ms * 1000),
      // _internal_key_data is default-initialized (member array)
      // Initialize time
      _last_scan_time(nil_time)
// Note: _decoder_address_pins and _col_pins vectors are default-initialized here
{
  // --- Runtime Input Validation ---
  // static_asserts in the header handle dimension range checks.
  // No need to check span size anymore.

  // Initialize the internal key data buffer
  // (Default initialization of KeyData structs might be sufficient,
  // but explicit zeroing is safer if KeyData changes)
  for (KeyData &key : _internal_key_data) {
    key = {}; // Default initialize KeyData structs
  }

  // Construct GpioPin objects in the vectors using the provided pin numbers
  for (uint pin_num : decoder_address_pins) {
    _decoder_address_pins.emplace_back(pin_num);
  }
  for (uint pin_num : col_pins) {
    _col_pins.emplace_back(pin_num);
  }
}

// --- init() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
void Keypad_HC138<NumRows, NumCols, Observers...>::init() {
  // Initialize Decoder Address Pins (Outputs)
  for (auto &pin : _decoder_address_pins) { // Iterate over GpioPin vector
    pin.set_direction(musin::hal::GpioDirection::OUT);
    pin.write(false); // Start with address 0
  }

  // Initialize Column Pins (Inputs with Pull-ups) - Iterate through the GpioPin vector
  for (auto &pin : _col_pins) { // Iterate over GpioPin vector
    pin.set_direction(musin::hal::GpioDirection::IN);
    pin.enable_pullup();
    // Optional: Enable hysteresis for potentially noisy inputs
    // pin.set_hysteresis(true); // Assuming a set_hysteresis method exists
  }

  _last_scan_time = get_absolute_time();
}

// --- scan() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
bool Keypad_HC138<NumRows, NumCols, Observers...>::scan() {
  absolute_time_t now = get_absolute_time();
  uint64_t diff_us = absolute_time_diff_us(_last_scan_time, now);

  if (diff_us < _scan_interval_us) {
    return false; // Not time to scan yet
  }
  _last_scan_time = now;

  // --- Clear transient flags before scan ---
  // Use the internal array for iteration
  for (KeyData &key : _internal_key_data) {
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
    sleep_us(2);

    for (std::uint8_t c = 0; c < NumCols; ++c) { // Use template parameter
      // Read raw state: LOW (false) means pressed (row LOW, col pulled HIGH)
      bool raw_key_pressed = !_col_pins[c].read(); // Use GpioPin::read()

      // Update state machine for this key
      update_key_state(r, c, raw_key_pressed, now);
    }
  }

  // Optional: De-select row (set address lines low or to an unused state)
  select_row(0); // Or select an address > num_rows if needed

  return true; // Scan was performed
}

// --- is_pressed() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
bool Keypad_HC138<NumRows, NumCols, Observers...>::is_pressed(std::uint8_t row,
                                                             std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols)
    return false; // Use template parameters
  const KeyState current_state =
      _internal_key_data[row * NumCols + col].state; // Use internal array
  return (current_state == KeyState::PRESSED || current_state == KeyState::HOLDING);
}

// --- was_pressed() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
bool Keypad_HC138<NumRows, NumCols, Observers...>::was_pressed(std::uint8_t row,
                                                              std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols)
    return false;                                              // Use template parameters
  return _internal_key_data[row * NumCols + col].just_pressed; // Use internal array
}

// --- was_released() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
bool Keypad_HC138<NumRows, NumCols, Observers...>::was_released(std::uint8_t row,
                                                               std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols)
    return false; // Use template parameters
  return _internal_key_data[row * NumCols + col].just_released; // Use internal array
}

// --- is_Held() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
bool Keypad_HC138<NumRows, NumCols, Observers...>::is_held(std::uint8_t row, std::uint8_t col) const {
  if (row >= NumRows || col >= NumCols)
    return false; // Use template parameters
  return (_internal_key_data[row * NumCols + col].state == KeyState::HOLDING); // Use internal array
}

// --- select_row() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
void Keypad_HC138<NumRows, NumCols, Observers...>::select_row(std::uint8_t row) {
  // Ensure row is within valid range for the decoder (0-7)
  // This check prevents issues even if _num_rows is smaller
  if (row >= 8)
    return;

  // Set A0, A1, A2 based on row number bits
  _decoder_address_pins[0].write((row >> 0) & 1); // A0 = LSB
  _decoder_address_pins[1].write((row >> 1) & 1); // A1
  _decoder_address_pins[2].write((row >> 2) & 1); // A2 = MSB
}

// --- update_key_state() Implementation ---
template <std::uint8_t NumRows, std::uint8_t NumCols, auto *...Observers>
void Keypad_HC138<NumRows, NumCols, Observers...>::update_key_state(std::uint8_t r, std::uint8_t c,
                                                                    bool raw_key_pressed,
                                                                    absolute_time_t now) {
  // Get mutable reference to the key's data using the internal array
  KeyData &key = _internal_key_data[r * NumCols + c]; // Use internal array

  switch (key.state) {
  case KeyState::IDLE:
    if (raw_key_pressed) {
      // Potential press detected, start debounce timer
      key.state = KeyState::DEBOUNCING_PRESS;
      key.press_start_time = now;
      key.state_change_time = now;
    }
    break;

  case KeyState::DEBOUNCING_PRESS:
    if (raw_key_pressed) {
      // Still pressed, check if debounce time has passed
      if (absolute_time_diff_us(key.state_change_time, now) >= _debounce_time_us) {
        // Debounce confirmed, transition to PRESSED
        KeyState next_state = KeyState::PRESSED;
        key.press_event_time = now;                   // Record press time for tap check
        key.just_pressed = true;                      // Set event flag
        notify_event(r, c, KeypadEvent::Type::Press); // Notify observers

        // Check immediately if hold time is zero or very small
        if (_hold_time_us == 0 ||
            absolute_time_diff_us(key.press_start_time, now) >= _hold_time_us) {
          next_state = KeyState::HOLDING;
          notify_event(r, c, KeypadEvent::Type::Hold); // Notify observers about hold
        }
        key.state = next_state;
      }
      // else: Debounce time not yet elapsed, remain in DEBOUNCING_PRESS
    } else {
      // Key released during debounce, return to IDLE
      key.state = KeyState::IDLE;
      key.press_start_time = nil_time;
      key.state_change_time = nil_time;
    }
    break;

  case KeyState::PRESSED:
    if (raw_key_pressed) {
      // Still pressed, check if hold time has passed
      if (absolute_time_diff_us(key.press_start_time, now) >= _hold_time_us) {
        key.state = KeyState::HOLDING;
        notify_event(r, c, KeypadEvent::Type::Hold); // Notify observers about hold
        // Note: press_start_time remains the original press time
      }
      // else: Hold time not yet elapsed, remain in PRESSED
    } else {
      // Potential release detected, start debounce timer
      key.state = KeyState::DEBOUNCING_RELEASE;
      key.state_change_time = now; // Record time release *started*
    }
    break;

  case KeyState::HOLDING:
    if (!raw_key_pressed) {
      // Potential release detected (from Hold state), start debounce timer
      key.state = KeyState::DEBOUNCING_RELEASE;
      key.state_change_time = now; // Record time release *started*
    }
    // else: Still Hold, remain in HOLDING state
    break;

  case KeyState::DEBOUNCING_RELEASE:
    if (!raw_key_pressed) {
      // Still released, check if debounce time has passed
      if (absolute_time_diff_us(key.state_change_time, now) >= _debounce_time_us) {
        // Debounce confirmed, transition to IDLE
        key.state = KeyState::IDLE;
        key.just_released = true;                       // Set event flag
        notify_event(r, c, KeypadEvent::Type::Release); // Notify observers

        // Check for tap event
        if (!is_nil_time(key.press_event_time)) {
          if (absolute_time_diff_us(key.press_event_time, now) < _tap_time_us) {
            notify_event(r, c, KeypadEvent::Type::Tap);
          }
        }
        key.press_event_time = nil_time;  // Reset for next press cycle
        key.press_start_time = nil_time;  // Reset for next state change
        key.state_change_time = nil_time; // Reset for next state change
      }
      // else: Debounce time not yet elapsed, remain in DEBOUNCING_RELEASE
    } else {
      // Key pressed again during release debounce. Go back to PRESSED.
      // The original press_start_time is still valid for hold calculations.
      key.state = KeyState::PRESSED;
      key.state_change_time = now; // Reset debounce timer to avoid immediate release.
    }
    break;
  } // end switch
}

// Note: Private member variables are defined in the header file.