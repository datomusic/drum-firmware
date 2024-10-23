#ifndef PITCH_SHIFTER_H_0GR8ZAHC
#define PITCH_SHIFTER_H_0GR8ZAHC

#include "sample_reader.h"
#include <stdint.h>

struct PitchShifter : SampleReader {
  PitchShifter(SampleReader &reader) : speed(1), reader(reader) {
  }

  // Reader interface
  void reset();

  // Reader interface
  bool has_data() {
    return reader.has_data();
  }

  // Reader interface
  uint32_t read_samples(int16_t *out, const uint16_t out_sample_count);

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
  double speed;
  SampleReader &reader;
  int16_t interpolationData[4];
};

#endif /* end of include guard: PITCH_SHIFTER_H_0GR8ZAHC */
