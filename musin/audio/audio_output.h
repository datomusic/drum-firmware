#ifndef AUDIO_OUTPUT_H_5M07E3OB
#define AUDIO_OUTPUT_H_5M07E3OB

#include "buffer_source.h"

namespace AudioOutput {
static const int SAMPLE_FREQUENCY = 44100;

/**
 * @brief Initializes the audio output system (I2S and Codec).
 * @return true on success, false on failure.
 */
bool init();

/**
 * @brief Fetches a block of audio from the source and sends it to the output.
 * @param source The BufferSource providing the audio data.
 * @return true if a buffer was successfully processed, false if no buffer was available.
 */
bool update(BufferSource &source);

/**
 * @brief Sets the master output volume of the audio codec.
 *
 * Maps the normalized float volume to the codec's hardware register range.
 * 0.0 corresponds to minimum volume (-63.5dB).
 * 1.0 corresponds to 0dB gain.
 * Values above 1.0 provide positive gain, up to the codec's maximum (+24dB).
 * Input values are clamped to the valid range representable by the codec.
 *
 * @param volume The desired volume level (0.0 to ~1.378).
 * @return true if the volume was set successfully, false otherwise (e.g., codec error).
 */
bool volume(float volume);

/**
 * @brief Routes the line input (IN1_L/R) directly to the headphone output (HPL/R).
 *
 * This uses the codec's analog bypass feature.
 * Requires the DATO_SUBMARINE build configuration (AIC3204 codec).
 *
 * @param enable true to enable routing, false to disable.
 * @return true if the routing was set successfully, false otherwise (e.g., codec error or feature unavailable).
 */
bool route_line_in_to_headphone(bool enable);

/**
 * @brief Deinitializes the audio output system.
 */
void deinit();

} // namespace AudioOutput

#endif /* end of include guard: AUDIO_OUTPUT_H_5M07E3OB */
