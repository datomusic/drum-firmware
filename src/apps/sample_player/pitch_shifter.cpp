
#include "pitch_shifter.h"

static int16_t fastinterpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4,
                               double x) {
  float x_1 = x * 1000.0;
  float x_2 = x_1 * x_1;
  float x_3 = x_2 * x_1;

  return d1 * (x_3 - 6000 * x_2 + 11000000 * x_1 - 6000000000) / -6000000000 +
         d2 * (x_3 - 5000 * x_2 + 6000000 * x_1) / 2000000000 +
         d3 * (x_3 - 4000 * x_2 + 3000000 * x_1) / -2000000000 +
         d4 * (x_3 - 3000 * x_2 + 2000000 * x_1) / 6000000000;
}

void PitchShifter::update(void) {
  audio_block_t *block;
  block = receiveWritable(0);
  if (block) {
    transmit(block);
    release(block);
  }
}

/*

const int numSamples = 8;
int16_t samples[numSamples] = {0, 1, 2, 3, 4, 5, 6, 7};
int16_t interpolationData[4] = {0, 0, 0, 0};

int main(int argc, char **argv) {
  double playbackRate = 0.1;
  double position = 0;
  int wholeNumber = 0;
  double remainder = 0;

  for (int i = 1; i < 4; i++) {
    interpolationData[i] = samples[i - 1];
  }

  while (wholeNumber < numSamples) {
    int interpolated = fastinterpolate(
        interpolationData[0], interpolationData[1], interpolationData[2],
        interpolationData[3], 1.0f + remainder);

    printf("sample[%d]: %d\t\t\tinterpolation = {%d, %d, %d, "
           "%d}\t\t\tremainder = %f === %d\n",
           wholeNumber, samples[wholeNumber], interpolationData[0],
           interpolationData[1], interpolationData[2], interpolationData[3],
           remainder, interpolated);

    int lastWholeNumber = wholeNumber;
    wholeNumber = floor(position);
    remainder = position - wholeNumber;
    position += playbackRate;

    if (wholeNumber - lastWholeNumber > 0) {
      interpolationData[0] = samples[lastWholeNumber];

      if (lastWholeNumber + 1 < numSamples)
        interpolationData[1] = samples[lastWholeNumber + 1];
      else
        interpolationData[1] = 0;

      if (lastWholeNumber + 2 < numSamples)
        interpolationData[2] = samples[lastWholeNumber + 2];
      else
        interpolationData[2] = 0;

      if (lastWholeNumber + 3 < numSamples)
        interpolationData[3] = samples[lastWholeNumber + 3];
      else
        interpolationData[3] = 0;
    }
  }
}

*/
