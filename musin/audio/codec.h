/**
 * @file codec.h
 * @brief Defines the abstract interface for audio codec drivers.
 */

#ifndef MUSIN_AUDIO_CODEC_H
#define MUSIN_AUDIO_CODEC_H

#include <cstdint> // For std::uint8_t, std::uint32_t

namespace Musin {
namespace Audio {

/**
 * @brief Abstract base class for audio codec hardware interfaces.
 *
 * Defines a common set of operations for interacting with audio codecs,
 * abstracting away the specific hardware details.
 */
class AudioCodec {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~AudioCodec() = default;

    /**
     * @brief Sets the main output volume.
     *
     * @param volume The desired volume level, typically normalized between
     *               0.0 (mute/minimum) and 1.0 (maximum/reference level).
     *               Implementations will map this to their specific hardware range.
     * @return true if the volume was set successfully, false otherwise.
     */
    virtual bool volume(float volume) = 0;

    // Add other common codec functions here as needed (e.g., mute, input gain, routing)
};

} // namespace Audio
} // namespace Musin

#endif // MUSIN_AUDIO_CODEC_H
