#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include "buffered_reader.h"
#include "sample_reader.h"
#include <stdint.h>

struct PitchShifter : SampleReader {

  constexpr PitchShifter(SampleReader &reader)
      : speed(1), sample_reader(reader), buffered_reader(reader) {
  }

  constexpr static int16_t quad_interpolate(const int16_t d1, const int16_t d2, const int16_t d3,
                                            const int16_t d4, const double x) {
    const float x_1 = x * 1000.0;
    const float x_2 = x_1 * x_1;
    const float x_3 = x_2 * x_1;

    const float result = d1 * (x_3 - 6000 * x_2 + 11000000 * x_1 - 6000000000) / -6000000000 +
                         d2 * (x_3 - 5000 * x_2 + 6000000 * x_1) / 2000000000 +
                         d3 * (x_3 - 4000 * x_2 + 3000000 * x_1) / -2000000000 +
                         d4 * (x_3 - 3000 * x_2 + 2000000 * x_1) / 6000000000;

    // Clamp the result to the valid range of int16_t before casting.
    // This prevents undefined behavior if the interpolated value goes out of bounds.
    if (result > INT16_MAX) {
      return INT16_MAX;
    }

    if (result < INT16_MIN) {
      return INT16_MIN;
    }

    return result;
  }

  // Reader interface
  constexpr void reset() override {
    for (int i = 0; i < 4; i++) {
      interpolation_samples[i] = 0;
    }

    position = 0;
    remainder = 0;
    source_index = 0;
    buffered_reader.reset();
  }

  // Reader interface
  constexpr bool has_data() override {
    return buffered_reader.has_data();
  }

  // Reader interface
  constexpr uint32_t read_samples(AudioBlock &out) override {
    if (this->speed < 1.01 && this->speed > 0.99) {
      return sample_reader.read_samples(out);
    } else {
      return read_resampled(out);
    }
  }

  constexpr void set_speed(const double speed) {
    if (speed < 0.2) {
      this->speed = 0.2;
    } else if (speed > 1.8) {
      this->speed = 1.8;
    } else {
      this->speed = speed;
    }
  }

private:
  constexpr uint32_t read_resampled(AudioBlock &out) {
    for (uint32_t out_sample_index = 0; out_sample_index < out.size(); ++out_sample_index) {

      const int32_t interpolated_value =
          quad_interpolate(interpolation_samples[0], interpolation_samples[1],
                           interpolation_samples[2], interpolation_samples[3], 1.0 + remainder);

      this->position += this->speed;
      const uint32_t new_source_index = (uint32_t)(position);

      while (source_index < new_source_index) {
        int16_t sample = 0;
        if (!buffered_reader.read_next(sample)) {
          sample = 0;
        }

        shift_interpolation_samples(sample);
        source_index++;
      }

      remainder = position - source_index;

      out[out_sample_index] = interpolated_value;
    }

    return out.size();
  }

  constexpr void shift_interpolation_samples(int16_t sample) {
    interpolation_samples[0] = interpolation_samples[1];
    interpolation_samples[1] = interpolation_samples[2];
    interpolation_samples[2] = interpolation_samples[3];
    interpolation_samples[3] = sample;
  }

  double speed;
  int16_t interpolation_samples[4];
  uint32_t source_index;
  double position;
  double remainder;
  SampleReader &sample_reader;
  BufferedReader buffered_reader;
};

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
