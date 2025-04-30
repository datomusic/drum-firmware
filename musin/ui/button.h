#ifndef MUSIN_UI_BUTTON_H
#define MUSIN_UI_BUTTON_H

#include "musin/hal/gpio.h"
#include "etl/observer.h"
#include <array>
#include <cstdint>

extern "C" {
#include "pico/time.h"
}

namespace Musin::UI {

class Button : public etl::observable<etl::observer<ButtonEvent>, 4> {
public:
    static constexpr uint32_t DEFAULT_DEBOUNCE_MS = 5;
    static constexpr uint32_t DEFAULT_HOLD_MS = 500;

    // Direct GPIO constructor
    Button(uint32_t gpio_pin, bool pull_up = true,
          uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_MS,
          uint32_t hold_time_ms = DEFAULT_HOLD_MS);

    // 8-channel mux constructor
    Button(uint32_t gpio_pin, const std::array<uint32_t,3>& mux_address_pins, uint8_t mux_channel,
          bool pull_up = true, uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_MS,
          uint32_t hold_time_ms = DEFAULT_HOLD_MS);

    // 16-channel mux constructor
    Button(uint32_t gpio_pin, const std::array<uint32_t,4>& mux_address_pins, uint8_t mux_channel,
          bool pull_up = true, uint32_t debounce_time_ms = DEFAULT_DEBOUNCE_MS,
          uint32_t hold_time_ms = DEFAULT_HOLD_MS);

    void init();
    bool update();

    bool is_pressed() const { return current_state != State::Idle; }
    bool was_pressed() const { return press_pending; }
    bool was_released() const { return release_pending; }
    bool is_held() const { return current_state == State::Held; }

private:
    enum class State {
        Idle,
        Pressed,
        DebouncingPress,
        Held,
        DebouncingRelease
    };

    void read_state();
    void set_mux_address() const;
    void handle_state_transition(bool raw_state, absolute_time_t now);

    Musin::HAL::GpioPin gpio;
    const bool pull_up;
    const uint32_t debounce_time_us;
    const uint32_t hold_time_us;
    
    // Mux configuration
    const bool is_muxed;
    const std::array<uint32_t,4> mux_address_pins;
    const uint8_t mux_channel;
    const uint8_t mux_width; // 0=direct, 3=8ch, 4=16ch

    // State tracking
    State current_state = State::Idle;
    absolute_time_t state_entered_time = nil_time;
    bool press_pending = false;
    bool release_pending = false;
};

} // namespace Musin::UI

#endif // MUSIN_UI_BUTTON_H
