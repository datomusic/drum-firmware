#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include "buffered_reader.h"
#include "sample_reader.h"
#include <stdint.h>

struct PitchShifter : SampleReader {
  PitchShifter(SampleReader &reader)
      : speed(1), sample_reader(reader), buffered_reader(reader) {
  }

  // Reader interface
  void reset();

  // Reader interface
  bool has_data() {
    return buffered_reader.has_data();
  }

  // Reader interface
  uint32_t read_samples(int16_t *out);

  void set_speed(const double speed) {
    if (speed < 0.2) {
      this->speed = 0.2;
    } else if (speed > 1.8) {
      this->speed = 1.8;
    } else {
      this->speed = speed;
    }
  }

private:
  uint32_t read_interpolated(int16_t *out);
  uint32_t read_simple(int16_t *out);

  double speed;
  int16_t interpolationData[4];
  SampleReader &sample_reader;
  BufferedReader buffered_reader;
};

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
