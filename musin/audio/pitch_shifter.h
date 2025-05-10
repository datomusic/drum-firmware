#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include <cstdint>

#include "port/section_macros.h"

#include "buffered_reader.h"
#include "sample_reader.h"

struct PitchShifter : SampleReader {
  constexpr PitchShifter(SampleReader &reader)
      : speed(1.0f), sample_reader(reader), buffered_reader(reader) {
    // Initialize interpolation buffer to avoid clicks/pops
    reset();
  }

  // Optimized cubic interpolation for ARM Cortex-M33 with FPU
  constexpr static int16_t
  __time_critical_func(cubic_interpolate)(const int16_t y0, const int16_t y1, const int16_t y2,
                                          const int16_t y3, const float mu) {
    // Cubic interpolation formula optimized for FPU
    const float mu2 = mu * mu;
    const float a0 = y3 - y2 - y0 + y1;
    const float a1 = y0 - y1 - a0;
    const float a2 = y2 - y0;
    const float a3 = y1;

    float result = a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;

    // Saturate result to int16_t range
    if (result > INT16_MAX) {
      return INT16_MAX;
    }
    if (result < INT16_MIN) {
      return INT16_MIN;
    }

    return static_cast<int16_t>(result);
  }

  // Reader interface
  constexpr void reset() override {
    // Pre-fill interpolation buffer with first sample or zeros
    // This helps prevent clicks/pops at the beginning
    int16_t first_sample = 0;
    // Try to get the first sample if available
    buffered_reader.reset();
    if (buffered_reader.has_data()) {
      buffered_reader.read_next(first_sample);
      buffered_reader.reset(); // Reset again after reading the first sample
    }

    for (int i = 0; i < 4; i++) {
      interpolation_samples[i] = first_sample;
    }

    position = 0.0f;
    remainder = 0.0f;
    source_index = 0;
  }

  // Reader interface
  constexpr bool has_data() override {
    return buffered_reader.has_data();
  }

  // Reader interface
  constexpr uint32_t read_samples(AudioBlock &out) override {
    // Fast path for essentially unmodified speed
    if (this->speed > 0.99f && this->speed < 1.01f) {
      return sample_reader.read_samples(out);
    } else {
      return read_resampled(out);
    }
  }

  constexpr void set_speed(const float new_speed) {
    // Clamp speed to valid range
    if (new_speed < 0.2f) {
      this->speed = 0.2f;
    } else if (new_speed > 2.0f) {
      this->speed = 2.0f;
    } else {
      this->speed = new_speed;
    }
  }

private:
  constexpr uint32_t __time_critical_func(read_resampled)(AudioBlock &out) {
    int16_t sample = 0;
    uint32_t samples_generated = 0;

    // Process each output sample
    for (uint32_t out_sample_index = 0; out_sample_index < out.size(); ++out_sample_index) {
      // Calculate interpolated value
      const int16_t interpolated_value =
          cubic_interpolate(interpolation_samples[0], interpolation_samples[1],
                            interpolation_samples[2], interpolation_samples[3], remainder);

      // Update position based on playback speed
      this->position += this->speed;
      const uint32_t new_source_index = static_cast<uint32_t>(position);

      // Advance source position and fill interpolation buffer as needed
      bool has_more_data = true;
      while (source_index < new_source_index && has_more_data) {
        has_more_data = buffered_reader.read_next(sample);
        if (!has_more_data) {
          sample = 0; // Use silence if we run out of data
        }

        shift_interpolation_samples(sample);
        source_index++;
      }

      // If we're completely out of data, stop generating samples
      if (!has_more_data && source_index > new_source_index + 4) {
        break;
      }

      // Calculate fractional position for next interpolation
      remainder = position - static_cast<float>(source_index);

      // Write the interpolated sample to output
      out[out_sample_index] = interpolated_value;
      samples_generated++;
    }

    return samples_generated;
  }

  constexpr void __time_critical_func(shift_interpolation_samples)(const int16_t sample) {
    // Shift samples in the interpolation buffer
    interpolation_samples[0] = interpolation_samples[1];
    interpolation_samples[1] = interpolation_samples[2];
    interpolation_samples[2] = interpolation_samples[3];
    interpolation_samples[3] = sample;
  }

  // Using float instead of double for better performance on Cortex-M33 FPU
  float speed;
// Align interpolation buffer to word boundary for better memory access
#ifdef TARGET_ARM_CORTEX_M
  alignas(4) int16_t interpolation_samples[4];
#else
  int16_t interpolation_samples[4];
#endif
  uint32_t source_index;
  float position;
  float remainder;
  SampleReader &sample_reader;
  BufferedReader buffered_reader;
};

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */