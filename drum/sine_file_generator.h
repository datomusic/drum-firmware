#ifndef DRUM_SINE_FILE_GENERATOR_H_
#define DRUM_SINE_FILE_GENERATOR_H_

#include "etl/array.h"
#include <cmath>
#include <cstdint>

extern "C" {
#include <stdio.h>
}

namespace drum {

/**
 * @brief Generates sine wave audio files for testing purposes.
 *
 * This class provides functionality to generate clean sine waves directly on
 * the device, bypassing the file transfer system. Useful for isolating audio
 * chain issues from transfer corruption problems.
 */
class SineFileGenerator {
public:
  static constexpr size_t DEFAULT_SAMPLE_RATE = 44100;
  static constexpr float DEFAULT_AMPLITUDE = 0.8f;
  static constexpr float DEFAULT_DURATION = 1.0f;

  constexpr SineFileGenerator() = default;

  /**
   * @brief Generates a sine wave and saves it to the specified file path.
   *
   * @param frequency Frequency in Hz (1.0f to 22050.0f)
   * @param file_path Path where the PCM file should be saved
   * @param duration Duration in seconds (default: 1.0s)
   * @param amplitude Amplitude (0.0f to 1.0f, default: 0.8f)
   * @param sample_rate Sample rate in Hz (default: 44100)
   * @return true if file was generated successfully, false otherwise
   */
  constexpr bool
  generate_sine_file(float frequency, const char *file_path,
                     float duration = DEFAULT_DURATION,
                     float amplitude = DEFAULT_AMPLITUDE,
                     size_t sample_rate = DEFAULT_SAMPLE_RATE) const {
    if (frequency <= 0.0f || frequency > static_cast<float>(sample_rate / 2)) {
      return false;
    }

    if (amplitude < 0.0f || amplitude > 1.0f) {
      return false;
    }

    if (duration <= 0.0f || duration > 10.0f) {
      return false;
    }

    const size_t num_samples = static_cast<size_t>(duration * sample_rate);

    FILE *file = fopen(file_path, "wb");
    if (!file) {
      return false;
    }

    // Generate and write samples in chunks to avoid large stack allocation
    constexpr size_t CHUNK_SIZE = 1024;
    etl::array<int16_t, CHUNK_SIZE> sample_buffer{};

    for (size_t start_sample = 0; start_sample < num_samples;
         start_sample += CHUNK_SIZE) {
      const size_t chunk_samples =
          etl::min(CHUNK_SIZE, num_samples - start_sample);

      // Generate chunk
      for (size_t i = 0; i < chunk_samples; ++i) {
        const size_t sample_index = start_sample + i;
        const float time =
            static_cast<float>(sample_index) / static_cast<float>(sample_rate);
        const float sample_value =
            std::sin(2.0f * M_PI * frequency * time) * amplitude;
        sample_buffer[i] = static_cast<int16_t>(sample_value * 32767.0f);
      }

      // Write chunk to file
      const size_t bytes_to_write = chunk_samples * sizeof(int16_t);
      const size_t bytes_written =
          fwrite(sample_buffer.data(), 1, bytes_to_write, file);

      if (bytes_written != bytes_to_write) {
        fclose(file);
        return false;
      }
    }

    fclose(file);
    return true;
  }

private:
};

} // namespace drum

#endif // DRUM_SINE_FILE_GENERATOR_H_