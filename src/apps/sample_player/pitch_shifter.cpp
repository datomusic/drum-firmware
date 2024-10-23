#include "pitch_shifter.h"

static int16_t quad_interpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                                double x) {
  float x_1 = x * 1000.0;
  float x_2 = x_1 * x_1;
  float x_3 = x_2 * x_1;

  return d1 * (x_3 - 6000 * x_2 + 11000000 * x_1 - 6000000000) / -6000000000 +
         d2 * (x_3 - 5000 * x_2 + 6000000 * x_1) / 2000000000 +
         d3 * (x_3 - 4000 * x_2 + 3000000 * x_1) / -2000000000 +
         d4 * (x_3 - 3000 * x_2 + 2000000 * x_1) / 6000000000;
}

void PitchShifter::reset() {
  for (int i = 0; i < 4; ++i) {
    this->interpolationData[i] = 0;
  }

  reader.reset();
}

uint32_t PitchShifter::read_samples(int16_t *out,
                                    const uint16_t out_sample_count) {
  // TODO: Stream in chunks instead of using a preallocated buffer.
  // Requires returning how many samples were read from reader.read_samples().
  static const uint32_t buffer_size = 256 * 10;
  int16_t samples[buffer_size];

  if (this->speed < 1.01 && this->speed > 0.99) {
    return reader.read_samples(out, out_sample_count);
  } else {
    uint32_t max_read_count = out_sample_count * this->speed;
    if (max_read_count > buffer_size) {
      max_read_count = buffer_size;
    }

    const uint32_t read_count = reader.read_samples(samples, max_read_count);

    for (int i = 1; i < 4; i++) {
      interpolationData[i] = samples[i - 1];
    }

    double position = 0;
    uint32_t wholeNumber = 0;
    double remainder = 0;

    uint32_t target_index;
    for (target_index = 0; target_index < out_sample_count; ++target_index) {

      *out++ = quad_interpolate(interpolationData[0], interpolationData[1],
                                interpolationData[2], interpolationData[3],
                                1.0 + remainder);

      uint32_t lastWholeNumber = wholeNumber;
      wholeNumber = (uint32_t)(position);
      remainder = position - wholeNumber;
      position += this->speed;

      if (wholeNumber - lastWholeNumber > 0) {
        interpolationData[0] = samples[lastWholeNumber];

        if (lastWholeNumber + 1 < read_count)
          interpolationData[1] = samples[lastWholeNumber + 1];
        else
          interpolationData[1] = 0;

        if (lastWholeNumber + 2 < read_count)
          interpolationData[2] = samples[lastWholeNumber + 2];
        else
          interpolationData[2] = 0;

        if (lastWholeNumber + 3 < read_count)
          interpolationData[3] = samples[lastWholeNumber + 3];
        else
          interpolationData[3] = 0;
      }
    }

    // Pad rest of buffer. This should only happen if reader.has_data() is false
    for (; target_index < out_sample_count; ++target_index) {
      *out++ = 0;
    }
    return out_sample_count;
  }
}
