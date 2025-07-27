#ifndef MUSIN_UI_BUTTON_H
#define MUSIN_UI_BUTTON_H

#include "etl/array.h"
#include "musin/hal/gpio.h"
#include "musin/observer.h"
#include <cstdint>

extern "C" {
#include "pico/time.h"
#include "hardware/gpio.h"
#include <cassert>
}

namespace musin::ui {

/**
 * @brief Event data structure for button notifications
 */
struct ButtonEvent {
  enum class Type : uint8_t { Press, Release, Hold };
  uint16_t button_id;
  Type type;
};

template <auto *...Observers> class Button {
public:
  static constexpr uint32_t DEFAULT_DEBOUNCE_MS = 5;
  static constexpr uint32_t DEFAULT_HOLD_MS = 500;

  // Direct GPIO constructor
  Button(uint32_t gpio_pin, bool active_high = false,
         uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_MS,
         uint32_t hold_time_ms = DEFAULT_HOLD_MS);

  /** @brief Get the button's unique ID based on its configuration */
  uint16_t get_id() const {
    return _id;
  }

  // 8-channel mux constructor
  Button(uint32_t gpio_pin, const etl::array<uint32_t, 3> &mux_address_pins, uint8_t mux_channel,
         bool pull_up = true, uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_MS,
         uint32_t hold_time_ms = DEFAULT_HOLD_MS);

  // 16-channel mux constructor
  Button(uint32_t gpio_pin, const etl::array<uint32_t, 4> &mux_address_pins, uint8_t mux_channel,
         bool pull_up = true, uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_MS,
         uint32_t hold_time_ms = DEFAULT_HOLD_MS);

  void init();
  bool update();

  bool is_pressed() const {
    return current_state != State::Idle;
  }
  bool was_pressed() const {
    return press_pending;
  }
  bool was_released() const {
    return release_pending;
  }
  bool is_held() const {
    return current_state == State::Held;
  }

private:
  enum class State { Idle, Pressed, Held, DebouncingRelease };

  void read_state();
  void set_mux_address() const;
  void handle_state_transition(bool raw_state, absolute_time_t now);
  void notify_observers(ButtonEvent::Type type);

  musin::hal::GpioPin gpio;
  const bool active_level; // True for active-high, false for active-low
  const uint32_t debounce_time_us;
  const uint32_t hold_time_us;

  // Mux configuration
  const bool is_muxed;
  const etl::array<uint32_t, 4> mux_address_pins;
  const uint8_t mux_channel;
  const uint8_t mux_width; // 0=direct, 3=8ch, 4=16ch

  // State tracking
  State current_state = State::Idle;
  absolute_time_t state_entered_time = nil_time;
  bool press_pending = false;
  bool release_pending = false;
  uint16_t _id; // Unique identifier for the button
};

// --- Implementation ---

template <auto *...Observers>
Button<Observers...>::Button(uint32_t gpio_pin, bool active_high, uint32_t debounce_time_ms,
                             uint32_t hold_time_ms)
    : gpio(gpio_pin), active_level(active_high), debounce_time_us(debounce_time_ms * 1000),
      hold_time_us(hold_time_ms * 1000), is_muxed(false), mux_address_pins{}, mux_channel(0),
      mux_width(0), _id(gpio_pin) // Direct connection ID is just the GPIO pin
{}

template <auto *...Observers>
Button<Observers...>::Button(uint32_t gpio_pin, const etl::array<uint32_t, 3> &mux_address_pins,
                             uint8_t mux_channel, bool active_high, uint32_t debounce_time_ms,
                             uint32_t hold_time_ms)
    : gpio(gpio_pin), active_level(active_high), debounce_time_us(debounce_time_ms * 1000),
      hold_time_us(hold_time_ms * 1000), is_muxed(true),
      mux_address_pins{mux_address_pins[0], mux_address_pins[1], mux_address_pins[2], 0},
      mux_channel(mux_channel), mux_width(3),
      _id((static_cast<uint16_t>(mux_channel) << 8) |
          gpio_pin) // Muxed ID combines pin and channel
{}

template <auto *...Observers>
Button<Observers...>::Button(uint32_t gpio_pin, const etl::array<uint32_t, 4> &mux_address_pins,
                             uint8_t mux_channel, bool active_high, uint32_t debounce_time_ms,
                             uint32_t hold_time_ms)
    : gpio(gpio_pin), active_level(active_high), debounce_time_us(debounce_time_ms * 1000),
      hold_time_us(hold_time_ms * 1000), is_muxed(true),
      mux_address_pins{mux_address_pins[0], mux_address_pins[1], mux_address_pins[2],
                       mux_address_pins[3]},
      mux_channel(mux_channel), mux_width(4),
      _id((static_cast<uint16_t>(mux_channel) << 8) |
          gpio_pin) // Muxed ID combines pin and channel
{}

template <auto *...Observers> void Button<Observers...>::init() {
  gpio.set_direction(musin::hal::GpioDirection::IN);

  // Configure pull based on active level
  if (active_level) {
    gpio.enable_pulldown();
  } else {
    gpio.enable_pullup();
  }

  // Initialize mux address pins if needed
  if (is_muxed) {
    for (int i = 0; i < mux_width; ++i) {
      musin::hal::GpioPin addr_pin(mux_address_pins[i]);
      addr_pin.set_direction(musin::hal::GpioDirection::OUT);
      addr_pin.write(false);
    }
  }
}

template <auto *...Observers> bool Button<Observers...>::update() {
  absolute_time_t now = get_absolute_time();
  bool raw_state = false;

  if (is_muxed) {
    set_mux_address();
    busy_wait_us(2); // Mux settling time
  }

  // Read physical state and invert if active low
  raw_state = gpio.read() ^ !active_level;

  handle_state_transition(raw_state, now);
  return press_pending || release_pending;
}

template <auto *...Observers> void Button<Observers...>::set_mux_address() const {
  if (mux_width == 3) { // 8-channel mux
    gpio_put(mux_address_pins[0], (mux_channel >> 0) & 1);
    gpio_put(mux_address_pins[1], (mux_channel >> 1) & 1);
    gpio_put(mux_address_pins[2], (mux_channel >> 2) & 1);
  } else if (mux_width == 4) { // 16-channel mux
    gpio_put(mux_address_pins[0], (mux_channel >> 0) & 1);
    gpio_put(mux_address_pins[1], (mux_channel >> 1) & 1);
    gpio_put(mux_address_pins[2], (mux_channel >> 2) & 1);
    gpio_put(mux_address_pins[3], (mux_channel >> 3) & 1);
  }
}

template <auto *...Observers>
void Button<Observers...>::handle_state_transition(bool raw_state, absolute_time_t now) {
  press_pending = false;
  release_pending = false;

  switch (current_state) {
  case State::Idle:
    if (raw_state) {
      current_state = State::Pressed;
      state_entered_time = now;
      press_pending = true;
      notify_observers(ButtonEvent::Type::Press);
    }
    break;

  case State::Pressed: {
    uint64_t time_pressed = absolute_time_diff_us(state_entered_time, now);

    if (!raw_state) {
      current_state = State::DebouncingRelease;
      state_entered_time = now;
    } else if (time_pressed >= hold_time_us) {
      current_state = State::Held;
      notify_observers(ButtonEvent::Type::Hold);
    }
    break;
  }

  case State::Held:
    if (!raw_state) {
      current_state = State::DebouncingRelease;
      state_entered_time = now;
    }
    break;

  case State::DebouncingRelease: {
    uint64_t time_debouncing = absolute_time_diff_us(state_entered_time, now);

    if (raw_state) { // Bounce detected
      current_state = State::Held;
    } else if (time_debouncing >= debounce_time_us) {
      current_state = State::Idle;
      release_pending = true;
      notify_observers(ButtonEvent::Type::Release);
    }
    break;
  }
  }
}

template <auto *...Observers> void Button<Observers...>::notify_observers(ButtonEvent::Type type) {
  musin::observable<Observers...>::notify_observers(ButtonEvent{_id, type});
}

} // namespace musin::ui

#endif // MUSIN_UI_BUTTON_H