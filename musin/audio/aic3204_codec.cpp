/**
 * @file aic3204_codec.cpp
 * @brief Implementation of the Aic3204Codec class.
 */

#include "aic3204_codec.h"
#include "drivers/aic3204.h" // Include the C header for implementation details
#include <stdexcept>         // For std::runtime_error
#include <cmath>             // For std::round
#include <algorithm>         // For std::clamp
#include <limits>            // For std::numeric_limits

namespace Musin {
namespace Audio {

Aic3204Codec::Aic3204Codec(std::uint8_t sda_pin, std::uint8_t scl_pin, std::uint32_t baudrate)
// Store parameters if needed: sda_pin_(sda_pin), scl_pin_(scl_pin), baudrate_(baudrate)
{
    if (!aic3204_init(sda_pin, scl_pin, baudrate)) {
        // Consider a more specific exception type if available/needed
        throw std::runtime_error("Failed to initialize AIC3204 codec");
    }
}

bool Aic3204Codec::volume(float volume) {
    // Clamp the input volume to the normalized range [0.0, 1.0]
    float clamped_volume = std::clamp(volume, 0.0f, 1.0f);

    // Map [0.0, 1.0] to the AIC3204 range [-127, 0] (0.5dB steps)
    // 0.0f -> -127 (-63.5 dB, effectively mute)
    // 1.0f -> 0 (0 dB)
    // Intermediate values are linearly mapped.
    constexpr float min_codec_val = -127.0f;
    constexpr float max_codec_val = 0.0f;
    float mapped_value = min_codec_val + clamped_volume * (max_codec_val - min_codec_val);

    // Round to the nearest integer step for the register value
    int8_t codec_register_value = static_cast<int8_t>(std::round(mapped_value));

    // Ensure the final value is within the valid int8_t range for safety,
    // although mapping logic should prevent exceeding [-127, 0].
    // The underlying C function also checks the range [-127, 48].
    codec_register_value = std::clamp(codec_register_value,
                                      static_cast<int8_t>(-127),
                                      static_cast<int8_t>(48)); // Clamp to full valid range just in case

    return aic3204_dac_set_volume(codec_register_value);
}

} // namespace Audio
} // namespace Musin
