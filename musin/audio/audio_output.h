#ifndef AUDIO_OUTPUT_H_5M07E3OB
#define AUDIO_OUTPUT_H_5M07E3OB

#include "buffer_source.h"

namespace musin::drivers {
class Aic3204;
}

namespace AudioOutput {
static const int SAMPLE_FREQUENCY = 44100;

/**
 * @brief Initializes the audio output system (I2S and Codec).
 *
 * This function must be called before any other audio functions. It configures
 * the I2S interface and initializes the audio codec using board-specific pin
 * configurations.
 *
 * @return true on success, false on failure.
 */
bool init();

/**
 * @brief Fetches a block of audio from the source and sends it to the output.
 * @param source The BufferSource providing the audio data.
 * @return true if a buffer was successfully processed, false if no buffer was
 * available.
 */
bool update(BufferSource &source);

/**
 * @brief Sets the master output volume of the audio codec.
 *
 * Maps a normalized float volume [0.0, 1.0] to the codec's hardware register
 * range using a fast quadratic approximation of a square root curve. This
 * provides finer control at higher volume levels with good performance. 0.0
 * corresponds to minimum volume (-63.5dB). 1.0 corresponds to maximum volume
 * (0dB gain). Input values are clamped to the [0.0, 1.0] range.
 *
 * @param volume The desired volume level (0.0 to 1.0).
 * @return true if the volume was set successfully, false otherwise (e.g., codec
 * error).
 */
bool volume(float volume);

/**
 * @brief Mutes all audio outputs by disabling amp and headphone drivers.
 *
 * Preserves the current volume setting and codec internal routing. Call
 * unmute() to restore audio output.
 *
 * @return true if muting was successful, false otherwise.
 */
bool mute();

/**
 * @brief Unmutes all audio outputs by enabling amp and headphone drivers.
 *
 * Restores audio output at the current volume setting.
 *
 * @return true if unmuting was successful, false otherwise.
 */
bool unmute();

/**
 * @brief Routes the line input (IN1_L/R) directly to the headphone output
 * (HPL/R).
 *
 * This uses the codec's analog bypass feature.
 * Requires the DATO_SUBMARINE build configuration (AIC3204 codec).
 *
 * @param enable true to enable routing, false to disable.
 * @return true if the routing was set successfully, false otherwise (e.g.,
 * codec error or feature unavailable).
 */
bool route_line_in_to_headphone(bool enable);

/**
 * @brief Checks if headphones are currently connected.
 *
 * @return true if headphones are connected, false if not connected or
 * detection unavailable.
 */
bool is_headphone_connected();

/**
 * @brief Deinitializes the audio output system.
 */
void deinit();

} // namespace AudioOutput

#endif /* end of include guard: AUDIO_OUTPUT_H_5M07E3OB */
