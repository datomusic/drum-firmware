#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include <AudioStream.h>
#include <stdint.h>

template <typename Reader> struct PitchShifter {
  PitchShifter() : speed(1) {
  }

  // Reader interface
  void init(const unsigned int *data, const uint32_t data_length) {
    for (int i = 0; i < 4; ++i) {
      this->interpolationData[i] = 0;
    }

    reader.init(data, data_length);
  }

  // Reader interface
  bool has_data() {
    return reader.has_data();
  }

  // Reader interface
  void read_samples(int16_t *out, const uint16_t out_sample_count);

  void set_speed(const double speed) {
    if (speed < 0.2) {
      this->speed = 0.2;
    } else if (speed > 1.8) {
      this->speed = 1.8;
    } else {
      this->speed = speed;
    }
  }

  Reader reader;

private:
  double speed;
  int16_t interpolationData[4];
};

namespace PitchShifterSupport {
int16_t quad_interpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                         double x);
}

template <typename Reader>
void PitchShifter<Reader>::read_samples(int16_t *out,
                                        const uint16_t out_sample_count) {
  // TODO: Stream in chunks instead of using a preallocated buffer.
  // Requires returning how many samples were read from reader.read_samples().
  static const uint32_t buffer_size = AUDIO_BLOCK_SAMPLES * 10;
  int16_t samples[buffer_size];

  if (this->speed < 1.01 && this->speed > 0.99) {
    reader.read_samples(out, out_sample_count);
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

      *out++ = PitchShifterSupport::quad_interpolate(
          interpolationData[0], interpolationData[1], interpolationData[2],
          interpolationData[3], 1.0 + remainder);

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

    for (; target_index < out_sample_count; ++target_index) {
      *out++ = 0;
    }
  }
}

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
