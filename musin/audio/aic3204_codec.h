/**
 * @file aic3204_codec.h
 * @brief C++ wrapper implementation for the AIC3204 audio codec driver.
 */

#ifndef MUSIN_AUDIO_AIC3204_CODEC_H
#define MUSIN_AUDIO_AIC3204_CODEC_H

#include "audio/codec.h"
#include <cstdint> // For std::uint8_t, std::uint32_t

// Forward declare C functions to avoid including the C header directly in the C++ header
// This improves encapsulation and reduces potential conflicts.
extern "C" {
    bool aic3204_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t baudrate);
    bool aic3204_dac_set_volume(int8_t volume);
    // Add forward declarations for other used C functions if needed
}

namespace Musin {
namespace Audio {

/**
 * @brief Concrete implementation of the AudioCodec interface for the TI AIC3204.
 *
 * This class wraps the C-style driver functions found in `drivers/aic3204.c`.
 */
class Aic3204Codec : public AudioCodec {
public:
    /**
     * @brief Constructs and initializes the AIC3204 codec driver.
     *
     * Calls the underlying C `aic3204_init` function.
     *
     * @param sda_pin The GPIO pin number for I2C SDA.
     * @param scl_pin The GPIO pin number for I2C SCL.
     * @param baudrate The desired I2C baudrate in Hz (e.g., 400000).
     * @throws std::runtime_error if initialization fails.
     */
    Aic3204Codec(std::uint8_t sda_pin, std::uint8_t scl_pin, std::uint32_t baudrate);

    /**
     * @brief Destructor (currently empty, C driver handles deinit if needed).
     */
    ~Aic3204Codec() override = default;

    // Prevent copying and assignment
    Aic3204Codec(const Aic3204Codec&) = delete;
    Aic3204Codec& operator=(const Aic3204Codec&) = delete;
    Aic3204Codec(Aic3204Codec&&) = delete;
    Aic3204Codec& operator=(Aic3204Codec&&) = delete;


    /**
     * @brief Sets the DAC output volume for the AIC3204.
     *
     * Maps the input float volume [0.0, 1.0] to the AIC3204's internal
     * register range [-127 (+-63.5dB) to 0 (0dB)].
     * Values outside [0.0, 1.0] are clamped.
     *
     * @param volume The desired volume level (0.0 to 1.0).
     * @return true if the volume was set successfully, false otherwise.
     */
    bool volume(float volume) override;

private:
    // Store configuration if needed later, e.g., for re-init
    // std::uint8_t sda_pin_;
    // std::uint8_t scl_pin_;
    // std::uint32_t baudrate_;
};

} // namespace Audio
} // namespace Musin

#endif // MUSIN_AUDIO_AIC3204_CODEC_H
