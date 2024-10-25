#ifndef QUAD_INTERPOLATOR_H_FWKJPH7R
#define QUAD_INTERPOLATOR_H_FWKJPH7R

#include "sample_reader.h"

struct QuadInterpolator : SampleReader {
  QuadInterpolator(SampleReader &reader) : sample_reader(reader) {
  }

  // Reader interface
  void reset();

  // Reader interface
  bool has_data() {
    return sample_reader.has_data();
  }

  // Reader interface
  uint32_t read_samples(int16_t *out);

private:
  int16_t interpolationData[4];
  SampleReader &sample_reader;
};

#endif /* end of include guard: QUAD_INTERPOLATOR_H_FWKJPH7R */
