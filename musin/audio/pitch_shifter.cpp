#include "pitch_shifter.h"

static int16_t quad_interpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                                float x) {
  float x_1 = x * 1000.0;
  float x_2 = x_1 * x_1;
  float x_3 = x_2 * x_1;

  return d1 * (x_3 - 6000 * x_2 + 11000000 * x_1 - 6000000000) / -6000000000 +
         d2 * (x_3 - 5000 * x_2 + 6000000 * x_1) / 2000000000 +
         d3 * (x_3 - 4000 * x_2 + 3000000 * x_1) / -2000000000 +
         d4 * (x_3 - 3000 * x_2 + 2000000 * x_1) / 6000000000;
}

void PitchShifter::reset() {
  for (int i = 1; i < 4; i++) {
    interpolation_samples[i] = 0;
  }

  position = 0;
  remainder = 0;
  source_index = 0;
  buffered_reader.reset();
}

uint32_t PitchShifter::read_samples(AudioBlock &out) {
  if (this->speed < 1.01 && this->speed > 0.99) {
    return sample_reader.read_samples(out);
  } else {
    return read_resampled(out);
  }
}

void PitchShifter::shift_interpolation_samples(int16_t sample) {
  interpolation_samples[0] = interpolation_samples[1];
  interpolation_samples[1] = interpolation_samples[2];
  interpolation_samples[2] = interpolation_samples[3];
  interpolation_samples[3] = sample;
}

uint32_t PitchShifter::read_resampled(AudioBlock &out) {
  for (uint32_t out_sample_index = 0; out_sample_index < AUDIO_BLOCK_SAMPLES;
       ++out_sample_index) {

    const int16_t interpolated_value = quad_interpolate(
        interpolation_samples[0], interpolation_samples[1],
        interpolation_samples[2], interpolation_samples[3], 1.0 + remainder);

    this->position += this->speed;
    const uint32_t new_source_index = (uint32_t)(position);

    while (source_index < new_source_index) {
      int16_t sample;
      if (!buffered_reader.read_next(sample)) {
        sample = 0;
      }

      shift_interpolation_samples(sample);
      source_index++;
    }

    remainder = position - source_index;

    out[out_sample_index] = interpolated_value;
  }

  return AUDIO_BLOCK_SAMPLES;
}
