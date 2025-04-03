#ifndef MUSIN_UI_DRUMPAD_H
#define MUSIN_UI_DRUMPAD_H

#include <cstdint>
#include <array>
#include <optional>

// Wrap C SDK headers
extern "C" {
#include "pico/time.h" // For absolute_time_t, time_us_64, etc.
#include "hardware/gpio.h"
#include "hardware/adc.h"
}

namespace Musin::UI {

/**
 * @brief Represents the possible states of a single drumpad sensor.
 */
enum class DrumpadState : std::uint8_t {
    IDLE,               ///< Pad is inactive, below noise threshold.
    RISING,             ///< Signal is rising, passed press threshold, waiting for velocity high threshold or peak.
    PEAKING,            ///< Signal has peaked or crossed velocity high threshold, velocity calculated.
    FALLING,            ///< Signal is falling after peaking.
    HOLDING,            ///< Signal remains above hold threshold after peaking for hold duration.
    DEBOUNCING_RELEASE  ///< Signal fell below release threshold, waiting for debounce confirmation.
};

/**
 * @brief Driver for a single analog drumpad sensor connected via an analog multiplexer.
 *
 * This driver reads an ADC channel after setting address lines for a multiplexer.
 * It handles debouncing, detects press, release, hold events, and calculates velocity
 * based on the time taken to cross two defined thresholds.
 */
class Drumpad {
public:
    // --- Constants ---
    // Default Thresholds (ADC values 0-4095 for 12-bit)
    static constexpr std::uint16_t DEFAULT_NOISE_THRESHOLD = 50;
    static constexpr std::uint16_t DEFAULT_PRESS_THRESHOLD = 100;      // Threshold to register a press start
    static constexpr std::uint16_t DEFAULT_VELOCITY_LOW_THRESHOLD = 150; // Lower threshold for velocity timing
    static constexpr std::uint16_t DEFAULT_VELOCITY_HIGH_THRESHOLD = 1000; // Upper threshold for velocity timing
    static constexpr std::uint16_t DEFAULT_RELEASE_THRESHOLD = 80;     // Threshold to register a release start
    static constexpr std::uint16_t DEFAULT_HOLD_THRESHOLD = 800;      // Threshold to maintain for hold state

    // Default Timings
    static constexpr std::uint32_t DEFAULT_SCAN_INTERVAL_US = 1000;    // 1ms default scan rate (adjust as needed)
    static constexpr std::uint32_t DEFAULT_DEBOUNCE_TIME_US = 5000;    // 5ms default release debounce time
    static constexpr std::uint32_t DEFAULT_HOLD_TIME_US = 200000;      // 200ms default hold time
    static constexpr std::uint64_t MAX_VELOCITY_TIME_US = 50000;       // Max time for velocity calc (slowest hit)
    static constexpr std::uint64_t MIN_VELOCITY_TIME_US = 500;         // Min time for velocity calc (fastest hit)


    /**
     * @brief Construct a new Drumpad driver instance.
     *
     * @param adc_pin The GPIO pin connected to the ADC input (must be ADC capable, e.g., 26, 27, 28).
     * @param address_pins Array containing the GPIO pin numbers for the multiplexer address lines (e.g., 3 pins for HC4051).
     * @param address_value The specific address value (0-7 for 3 address pins) for this drumpad on the multiplexer.
     * @param noise_threshold ADC value below which the input is considered idle noise.
     * @param press_threshold ADC value above which a potential press is detected.
     * @param velocity_low_threshold Lower ADC threshold for velocity timing window.
     * @param velocity_high_threshold Upper ADC threshold for velocity timing window.
     * @param release_threshold ADC value below which a potential release is detected (after being pressed).
     * @param hold_threshold ADC value that must be maintained to enter the HOLDING state.
     * @param scan_interval_us Time between ADC reads in microseconds.
     * @param debounce_time_us Time duration for debouncing release transitions in microseconds.
     * @param hold_time_us Minimum time the pad must be above hold_threshold after peaking to be considered 'held'.
     */
    Drumpad(uint adc_pin,
            const std::vector<uint>& address_pins, // Use vector for flexibility in address pin count
            uint8_t address_value,
            std::uint16_t noise_threshold = DEFAULT_NOISE_THRESHOLD,
            std::uint16_t press_threshold = DEFAULT_PRESS_THRESHOLD,
            std::uint16_t velocity_low_threshold = DEFAULT_VELOCITY_LOW_THRESHOLD,
            std::uint16_t velocity_high_threshold = DEFAULT_VELOCITY_HIGH_THRESHOLD,
            std::uint16_t release_threshold = DEFAULT_RELEASE_THRESHOLD,
            std::uint16_t hold_threshold = DEFAULT_HOLD_THRESHOLD,
            std::uint32_t scan_interval_us = DEFAULT_SCAN_INTERVAL_US,
            std::uint32_t debounce_time_us = DEFAULT_DEBOUNCE_TIME_US,
            std::uint32_t hold_time_us = DEFAULT_HOLD_TIME_US);

    // Prevent copying and assignment
    Drumpad(const Drumpad&) = delete;
    Drumpad& operator=(const Drumpad&) = delete;

    /**
     * @brief Initialize GPIO pins for address lines and the ADC.
     * Must be called once before starting updates.
     */
    void init();

    /**
     * @brief Performs an update cycle if the scan interval has elapsed.
     * This function should be called periodically. It sets the mux address,
     * reads the ADC, updates the internal state machine, and detects events.
     * @return true if an update was performed, false otherwise.
     */
    bool update();

    /**
     * @brief Checks if the pad transitioned to a pressed state during the *last completed* update cycle.
     * This flag is cleared at the beginning of the next update.
     * @return true if the pad was just pressed, false otherwise.
     */
    bool was_pressed() const { return _just_pressed; }

    /**
     * @brief Checks if the pad transitioned back to the IDLE state (was released) during the *last completed* update cycle.
     * This flag is cleared at the beginning of the next update.
     * @return true if the pad was just released, false otherwise.
     */
    bool was_released() const { return _just_released; }

    /**
     * @brief Checks if the pad is currently in the HOLDING state.
     * @return true if the pad state is HOLDING, false otherwise.
     */
    bool is_held() const { return _current_state == DrumpadState::HOLDING; }

    /**
     * @brief Gets the calculated velocity (0-127) if a press occurred during the *last completed* update cycle.
     * The velocity is calculated based on the time taken to rise between velocity_low_threshold and velocity_high_threshold.
     * Faster rise time results in higher velocity.
     * @return An optional containing the velocity (0-127) if `was_pressed()` is true, otherwise `std::nullopt`.
     */
    std::optional<uint8_t> get_velocity() const { return _last_velocity; }

    /** @brief Get the current raw ADC reading from the last update. */
    std::uint16_t get_raw_adc_value() const { return _last_adc_value; }

    /** @brief Get the current state of the pad's state machine. */
    DrumpadState get_current_state() const { return _current_state; }


private:
    /**
     * @brief Sets the multiplexer address pins based on the configured address value.
     */
    void set_address_pins();

    /**
     * @brief Reads the ADC value for the configured ADC pin.
     * Assumes ADC is initialized and the correct channel is selected externally if needed (beyond mux).
     * @return The 12-bit ADC reading.
     */
    std::uint16_t read_adc();

    /**
     * @brief Updates the internal state machine based on the new ADC reading and timings.
     * @param current_adc_value The latest ADC reading.
     * @param now The current time.
     */
    void update_state_machine(std::uint16_t current_adc_value, absolute_time_t now);

    /**
     * @brief Calculates velocity based on the time difference.
     * @param time_diff_us Time difference in microseconds between crossing low and high thresholds.
     * @return Velocity value (0-127).
     */
    uint8_t calculate_velocity(uint64_t time_diff_us) const;

    // --- Configuration (initialized in constructor) ---
    const uint _adc_pin;
    const uint _adc_channel; // Derived from adc_pin
    const std::vector<uint> _address_pins;
    const uint8_t _address_value;
    const std::uint16_t _noise_threshold;
    const std::uint16_t _press_threshold;
    const std::uint16_t _velocity_low_threshold;
    const std::uint16_t _velocity_high_threshold;
    const std::uint16_t _release_threshold;
    const std::uint16_t _hold_threshold;
    const std::uint32_t _scan_interval_us;
    const std::uint32_t _debounce_time_us;
    const std::uint32_t _hold_time_us;

    // --- State ---
    DrumpadState _current_state = DrumpadState::IDLE;
    std::uint16_t _last_adc_value = 0;
    absolute_time_t _last_update_time = nil_time;
    absolute_time_t _state_transition_time = nil_time; // Time the current state was entered
    absolute_time_t _velocity_low_time = nil_time;     // Time velocity low threshold was crossed
    absolute_time_t _velocity_high_time = nil_time;    // Time velocity high threshold was crossed

    // Event flags (cleared at the start of each update)
    bool _just_pressed = false;
    bool _just_released = false;
    std::optional<uint8_t> _last_velocity = std::nullopt;

    bool _initialized = false;

}; // class Drumpad

} // namespace Musin::UI

#endif // MUSIN_UI_DRUMPAD_H
