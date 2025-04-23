#ifndef MUSIN_UI_DRUMPAD_H
#define MUSIN_UI_DRUMPAD_H

#include <cstdint>
#include <optional>
#include <type_traits> // For static_assert

#include "etl/observer.h" // Include ETL observer pattern
#include "musin/hal/analog_in.h"

extern "C" {
#include "pico/time.h"
}

namespace Musin::UI {

struct DrumpadEvent {
    enum class Type : uint8_t {
        Press,
        Release,
        Hold // Optional: Add if needed
    };
    uint8_t pad_index;
    Type type;
    std::optional<uint8_t> velocity; // Present for Press events
    uint16_t raw_value;              // Current raw ADC value
};

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
 * based on the time taken to cross two defined thresholds using an external analog reader.
 *
 * @tparam AnalogReader Type of the analog input object (e.g., AnalogIn, AnalogInMux8).
 *                      Must provide a `read_raw()` method returning std::uint16_t.
 */
template <typename AnalogReader>
class Drumpad : public etl::observable<etl::observer<DrumpadEvent>, 4> { // Inherit from observable
public:
    // Type check for the AnalogReader
    // Concept would be better in C++20, but static_assert works for C++17
    // This check is basic; it doesn't guarantee the method signature perfectly.
    static_assert(std::is_member_function_pointer_v<decltype(&AnalogReader::read_raw)>,
                  "AnalogReader must have a 'read_raw' method.");
    // Could add more checks, e.g., for return type if needed, but gets complex.

    // --- Constants ---
    // Default Thresholds (ADC values 0-4095 for 12-bit)
    static constexpr std::uint16_t DEFAULT_NOISE_THRESHOLD = 50;
    static constexpr std::uint16_t DEFAULT_PRESS_THRESHOLD = 100;      // Threshold to register a press start
    static constexpr std::uint16_t DEFAULT_VELOCITY_LOW_THRESHOLD = 150; // Lower threshold for velocity timing
    static constexpr std::uint16_t DEFAULT_VELOCITY_HIGH_THRESHOLD = 3000; // Upper threshold for velocity timing
    static constexpr std::uint16_t DEFAULT_RELEASE_THRESHOLD = 100;     // Threshold to register a release start
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
     * @param pad_index A unique index for this drumpad, used in events.
     */
    explicit Drumpad(AnalogReader& reader, // Accept reference to the analog reader
                     uint8_t pad_index,    // Add pad index
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

    // No init() needed here anymore, assumes reader is initialized externally.

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

    /** @brief [DEBUG] Get the last calculated time difference for velocity. */
    uint64_t get_last_velocity_time_diff() const { return _last_velocity_time_diff; }


private:
    // Removed set_address_pins() and read_adc()

    /**
     * @brief Notifies observers with a DrumpadEvent.
     * @param type The type of event (Press, Release, Hold).
     * @param velocity Optional velocity for Press events.
     * @param raw_value The current raw ADC value.
     */
    void notify_event(DrumpadEvent::Type type, std::optional<uint8_t> velocity, uint16_t raw_value);


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
    AnalogReader& _reader;
    const uint8_t _pad_index; // Store the pad index
    // Removed ADC/address pin members
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
    uint64_t _last_velocity_time_diff = 0; // DEBUG: Store last time diff

    // Removed _initialized flag, relies on external reader initialization

}; // class Drumpad


// ============================================================================
// Template Implementation
// ============================================================================

// --- Constructor Implementation ---
template <typename AnalogReader>
Drumpad<AnalogReader>::Drumpad(AnalogReader& reader,
                               uint8_t pad_index, // Add pad_index to constructor
                               std::uint16_t noise_threshold,
                               std::uint16_t press_threshold,
                               std::uint16_t velocity_low_threshold,
                               std::uint16_t velocity_high_threshold,
                               std::uint16_t release_threshold,
                               std::uint16_t hold_threshold,
                               std::uint32_t scan_interval_us,
                               std::uint32_t debounce_time_us,
                               std::uint32_t hold_time_us) :
    _reader(reader), // Store reference to the reader
    _pad_index(pad_index), // Initialize pad index
    _noise_threshold(noise_threshold),
    _press_threshold(press_threshold),
    _velocity_low_threshold(velocity_low_threshold),
    _velocity_high_threshold(velocity_high_threshold),
    _release_threshold(release_threshold),
    _hold_threshold(hold_threshold),
    _scan_interval_us(scan_interval_us),
    _debounce_time_us(debounce_time_us),
    _hold_time_us(hold_time_us),
    _current_state(DrumpadState::IDLE),
    _last_adc_value(0),
    _last_update_time(nil_time),
    _state_transition_time(nil_time),
    _velocity_low_time(nil_time),
    _velocity_high_time(nil_time),
    _just_pressed(false),
    _just_released(false),
    _last_velocity(std::nullopt)
{}

// --- Update Method ---
template <typename AnalogReader>
bool Drumpad<AnalogReader>::update() {
    absolute_time_t now = get_absolute_time();
    uint64_t time_since_last_update = absolute_time_diff_us(_last_update_time, now);

    if (is_nil_time(_last_update_time) || time_since_last_update >= _scan_interval_us) {
        // Clear event flags from the previous cycle
        _just_pressed = false;
        _just_released = false;
        _last_velocity = std::nullopt;

        // Read the raw ADC value using the provided reader
        std::uint16_t raw_adc_value = _reader.read_raw();

        // --- Invert the reading for falling edge hardware ---
        // Assuming 12-bit ADC (0-4095)
        constexpr std::uint16_t ADC_MAX_VALUE = 4095;
        std::uint16_t current_adc_value = ADC_MAX_VALUE - raw_adc_value;
        // ----------------------------------------------------

        _last_adc_value = current_adc_value; // Store the *inverted* value for state machine

        update_state_machine(current_adc_value, now); // Use the inverted value

        _last_update_time = now;
        return true; // Update was performed
    }
    return false; // Scan interval not elapsed
}


// --- State Machine Update ---
template <typename AnalogReader>
void Drumpad<AnalogReader>::update_state_machine(std::uint16_t current_adc_value, absolute_time_t now) {
    uint64_t time_in_state = absolute_time_diff_us(_state_transition_time, now);

    switch (_current_state) {
        case DrumpadState::IDLE:
            if (current_adc_value >= _press_threshold) {
                _current_state = DrumpadState::RISING;
                _state_transition_time = now;
                _velocity_low_time = nil_time;
                _velocity_high_time = nil_time;
            }
            break;

        case DrumpadState::RISING:
            // Check for velocity low threshold crossing
            if (is_nil_time(_velocity_low_time) && current_adc_value >= _velocity_low_threshold) {
                _velocity_low_time = now;
            }
            // Check for velocity high threshold crossing
            if (!is_nil_time(_velocity_low_time) && current_adc_value >= _velocity_high_threshold) {
                 _velocity_high_time = now;
                 _current_state = DrumpadState::PEAKING;
                 _state_transition_time = now;

                 uint64_t diff = absolute_time_diff_us(_velocity_low_time, _velocity_high_time);
                 _last_velocity_time_diff = diff; // DEBUG: Store time diff
                 _last_velocity = calculate_velocity(diff);
                 _just_pressed = true;
                 notify_event(DrumpadEvent::Type::Press, _last_velocity, current_adc_value); // Notify press
            } else if (current_adc_value < _release_threshold) {
                 // Signal dropped below release threshold before hitting velocity high
                 _current_state = DrumpadState::DEBOUNCING_RELEASE;
                 _state_transition_time = now;
            }
            // Add a timeout? If it stays in RISING too long without hitting high threshold?
            break;

       case DrumpadState::PEAKING:
            if (current_adc_value < _velocity_high_threshold) {
                _current_state = DrumpadState::FALLING;
            }
            if (current_adc_value >= _hold_threshold && time_in_state >= _hold_time_us) {
                _current_state = DrumpadState::HOLDING;
            }
            break;

       case DrumpadState::FALLING:
           if (current_adc_value < _release_threshold) {
               _current_state = DrumpadState::DEBOUNCING_RELEASE;
               _state_transition_time = now;
           } else if (current_adc_value >= _hold_threshold && time_in_state >= _hold_time_us) {
                // Check if it went back up and met hold criteria
                _current_state = DrumpadState::HOLDING;
                // Keep _state_transition_time from PEAKING entry
            }
            // Could potentially go back to RISING/PEAKING if signal increases significantly again? (Retrigger logic - complex)
            break;

       case DrumpadState::HOLDING:
           if (current_adc_value < _release_threshold) { // Check against release, not hold threshold, to start release
               _current_state = DrumpadState::DEBOUNCING_RELEASE;
               _state_transition_time = now;
           }
           break;

        case DrumpadState::DEBOUNCING_RELEASE:
            if (current_adc_value >= _release_threshold) {
                // Bounced back up - return to previous relevant state.
                // Need to know if we came from HOLDING or FALLING/RISING.
                // Simplification: Go back to FALLING, assuming it won't bounce high enough for HOLDING immediately.
                // Simplification: Go back to FALLING. More robust: store previous state.
                _current_state = DrumpadState::FALLING;
                _state_transition_time = now;
           } else if (time_in_state >= _debounce_time_us) {
               // Debounce time elapsed, confirm release
               _current_state = DrumpadState::IDLE;
               _state_transition_time = now;
               _just_released = true;
               _last_adc_value = 0;
               _velocity_low_time = nil_time;
               _velocity_high_time = nil_time;
            }
            break;
    }
}

// --- Velocity Calculation ---
template <typename AnalogReader>
uint8_t Drumpad<AnalogReader>::calculate_velocity(uint64_t time_diff_us) const {
    // Clamp time difference to expected range
    if (time_diff_us <= MIN_VELOCITY_TIME_US) {
        return 127; // Max velocity for fastest hits
    }
    if (time_diff_us >= MAX_VELOCITY_TIME_US) {
        return 1; // Min velocity for slowest hits (or 0?)
    }

    // Inverse linear mapping: velocity = 127 * (1 - (time - min_time) / (max_time - min_time))
    // Scale to 1-127 range
    uint64_t time_range = MAX_VELOCITY_TIME_US - MIN_VELOCITY_TIME_US;
    uint64_t adjusted_time = time_diff_us - MIN_VELOCITY_TIME_US;

    // Calculate velocity (floating point for intermediate step might be simpler)
    // float velocity_f = 1.0f + 126.0f * (1.0f - (float)adjusted_time / (float)time_range);
    // return static_cast<uint8_t>(velocity_f);

    // Integer calculation:
    // velocity = 1 + 126 * (time_range - adjusted_time) / time_range
    // Use 64-bit intermediate to avoid overflow
    uint64_t velocity_scaled = 126ULL * (time_range - adjusted_time);
    uint8_t velocity = 1 + static_cast<uint8_t>(velocity_scaled / time_range);

    return velocity; // Range 1-127
}


} // namespace Musin::UI

#endif // MUSIN_UI_DRUMPAD_H
