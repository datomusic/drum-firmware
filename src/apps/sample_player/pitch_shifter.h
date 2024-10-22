#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include <AudioStream.h>
#include <stdint.h>

namespace PitchShifterSupport {
int16_t quad_interpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                         double x);
}

template <typename Reader> struct PitchShifter {
  PitchShifter() : speed(1.5) {
  }

  // Reader interface
  void init(const unsigned int *data, const uint32_t data_length) {
    reader.init(data, data_length);
  }

  // Reader interface
  bool has_data() {
    return reader.has_data();
  }

  // Reader interface
  void read_samples(int16_t *out, const uint16_t out_sample_count);

  void set_speed(const float speed) {
    if (speed < 0.1f) {
      this->speed = 0.1;
    } else if (speed > 2) {
      this->speed = 2;
    } else {
      this->speed = speed;
    }
  }

  Reader reader;

private:
  float speed;
};

template <typename Reader>
void PitchShifter<Reader>::read_samples(int16_t *out,
                                        const uint16_t out_sample_count) {
  // TODO: Stream in chunks instead of using a preallocated buffer.
  // Requires returning how many samples were read from reader.read_samples().
  static const uint32_t buffer_size = AUDIO_BLOCK_SAMPLES * 2;
  int16_t buffer[buffer_size];

  if (this->speed < 1.01f && this->speed > 0.99f) {
    reader.read_samples(out, out_sample_count);
  } else {
    uint32_t read_count = out_sample_count * this->speed;
    if (read_count > buffer_size) {
      read_count = buffer_size;
    }

    reader.read_samples(buffer, read_count);
    for (int target_index = 0; target_index < out_sample_count;
         ++target_index) {
      const uint32_t source_index = (int)(target_index * this->speed);
      if (source_index < buffer_size) {

        *out++ = buffer[source_index];
      } else {
        *out++ = 0;
      }
    }
  }
}

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
