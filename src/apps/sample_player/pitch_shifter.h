#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include <stdint.h>

template <typename Reader> struct PitchShifter {
  PitchShifter() {
  }

  void init(const unsigned int *data) {
    reader.init(data);
  }

  bool has_data() {
    return reader.has_data();
  }

  void read_samples(int16_t *out, const uint16_t count) {
    reader.read_samples(out, count);
  }

private:
  Reader reader;
};

namespace PitchShifterSupport {
int16_t quad_interpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                         double x);
}

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
