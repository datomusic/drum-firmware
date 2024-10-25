#include "pitch_shifter.h"

void PitchShifter::reset() {
  for (int i = 0; i < 4; ++i) {
    this->interpolationData[i] = 0;
  }

  buffered_reader.reset();
}

uint32_t PitchShifter::read_samples(int16_t *out) {
  if (this->speed < 1.01 && this->speed > 0.99) {
    return sample_reader.read_samples(out);
  } else {
    return read_resampled(out);
  }
}

uint32_t PitchShifter::read_resampled(int16_t *out) {
  uint32_t source_index = 0;

  int16_t sample;
  buffered_reader.read_next(sample);

  for (uint32_t target_index = 0; target_index < AUDIO_BLOCK_SAMPLES;
       ++target_index) {
    const double position = target_index * this->speed;
    const uint32_t new_source_index = (uint32_t)(position);
    while (source_index < new_source_index) {
      source_index++;
      if (!buffered_reader.read_next(sample)) {
        sample = 0;
      }
    }

    *out = sample;
    out++;
  }

  return AUDIO_BLOCK_SAMPLES;
}
