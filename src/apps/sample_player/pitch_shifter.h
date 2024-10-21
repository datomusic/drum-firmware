#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include <AudioStream.h>
#include <stdint.h>

template <typename Reader> struct PitchShifter {
  PitchShifter() : speed(0.9f) {
  }

  // Reader interface
  void init(const unsigned int *data, const uint32_t data_length){
    reader.init(data, data_length);
  }

  // Reader interface
  bool has_data() {
    return reader.has_data();
  }

  // Reader interface
  void read_samples(int16_t *out, const uint16_t out_sample_count) {
    static const uint32_t buffer_size = AUDIO_BLOCK_SAMPLES * 2;
    int16_t buffer[buffer_size];

    if (this->speed > 1) {
      for (uint32_t i = 0; i < buffer_size; ++i) {
        buffer[i] = 0;
      }

      uint32_t read_count = ((out_sample_count * this->speed) / 2) * 2;
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

    } else {
      reader.read_samples(out, out_sample_count);
      /*
      int16_t buffer[8];
      int16_t interpolationData[4] = {0, 0, 0, 0};

      int counter = 0;
      while (counter < sample_count) {
        reader.read_samples(buffer, 8);
        *out++ = buffer[0];
        *out++ = buffer[2];
        *out++ = buffer[4];
        *out++ = buffer[6];
        counter += 8;
      }
      */
    }
  }

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

namespace PitchShifterSupport {
int16_t quad_interpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                         double x);
}

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
