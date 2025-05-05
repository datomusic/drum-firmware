#include "musin/ui/button.h"
#include <cassert>

extern "C" {
#include "hardware/gpio.h"
#include "pico/time.h"
}

namespace musin::ui {

Button::Button(uint32_t gpio_pin, bool active_high,
             uint32_t debounce_time_ms,
             uint32_t hold_time_ms) :
    gpio(gpio_pin),
    active_level(active_high),
    debounce_time_us(debounce_time_ms * 1000),
    hold_time_us(hold_time_ms * 1000),
    is_muxed(false),
    mux_address_pins{},
    mux_channel(0),
    mux_width(0),
    _id(gpio_pin) // Direct connection ID is just the GPIO pin
{
}

Button::Button(uint32_t gpio_pin, const std::array<uint32_t,3>& mux_address_pins, 
             uint8_t mux_channel, bool active_high,
             uint32_t debounce_time_ms, uint32_t hold_time_ms) :
    gpio(gpio_pin),
    active_level(active_high),
    debounce_time_us(debounce_time_ms * 1000),
    hold_time_us(hold_time_ms * 1000),
    is_muxed(true),
    mux_address_pins{mux_address_pins[0], mux_address_pins[1], mux_address_pins[2], 0},
    mux_channel(mux_channel),
    mux_width(3),
    _id((static_cast<uint16_t>(mux_channel) << 8) | gpio_pin) // Muxed ID combines pin and channel
{
}

Button::Button(uint32_t gpio_pin, const std::array<uint32_t,4>& mux_address_pins,
             uint8_t mux_channel, bool active_high,
             uint32_t debounce_time_ms, uint32_t hold_time_ms) :
    gpio(gpio_pin),
    active_level(active_high),
    debounce_time_us(debounce_time_ms * 1000),
    hold_time_us(hold_time_ms * 1000),
    is_muxed(true),
    mux_address_pins{mux_address_pins[0], mux_address_pins[1], mux_address_pins[2], mux_address_pins[3]},
    mux_channel(mux_channel),
    mux_width(4),
    _id((static_cast<uint16_t>(mux_channel) << 8) | gpio_pin) // Muxed ID combines pin and channel
{
}

void Button::init() {
    gpio.set_direction(musin::hal::GpioDirection::IN);
    
    // Configure pull based on active level
    if (active_level) {
        gpio.enable_pulldown();
    } else {
        gpio.enable_pullup();
    }

    // Initialize mux address pins if needed
    if (is_muxed) {
        for(int i = 0; i < mux_width; ++i) {
            musin::hal::GpioPin addr_pin(mux_address_pins[i]);
            addr_pin.set_direction(musin::hal::GpioDirection::OUT);
            addr_pin.write(false);
        }
    }
}

bool Button::update() {
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

void Button::set_mux_address() const {
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

void Button::handle_state_transition(bool raw_state, absolute_time_t now) {
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

void Button::notify_observers(ButtonEvent::Type type) {
    etl::observable<etl::observer<ButtonEvent>, 4>::notify_observers(ButtonEvent{_id, type});
}

} // namespace musin::ui
