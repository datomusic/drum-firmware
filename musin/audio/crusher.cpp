#include "crusher.h"

void Crusher::crush(AudioBlock &samples) {
  uint32_t i;
  uint32_t sampleSquidge;
  uint32_t sampleSqueeze; // squidge is bitdepth, squeeze is for samplerate

  if (sampleStep <= 1) { // no sample rate mods, just crush the bitdepth.
    for (i = 0; i < samples.size(); i++) {
      // shift bits right to cut off fine detail sampleSquidge is a
      // uint32 so sign extension will not occur, fills with zeroes.
      sampleSquidge = samples[i] >> (16 - crushBits);

      // shift bits left again to regain the volume level.
      // fills with zeroes.
      samples[i] = sampleSquidge << (16 - crushBits);
    }
  } else if (crushBits == 16) {
    // bitcrusher not being used, samplerate mods only.
    i = 0;
    while (i < samples.size()) {
      // save the root sample. this will pick up a root
      // sample every _sampleStep_ samples.
      sampleSqueeze = samples[i]; // block->data[i];
      for (int j = 0; j < sampleStep && i < samples.size(); j++) {
        // for each repeated sample, paste in the current
        // root sample, then move onto the next step.
        /* block->data[i] */ samples[i] = sampleSqueeze;
        i++;
      }
    }
  } else { // both being used. crush those bits and mash those samples.
    i = 0;
    while (i < samples.size()) {
      // save the root sample. this will pick up a root sample
      // every _sampleStep_ samples.
      sampleSqueeze = samples[i]; // block->data[i];
                                  //
      for (int j = 0; j < sampleStep && i < samples.size(); j++) {
        // shift bits right to cut off fine detail sampleSquidge
        // is a uint32 so sign extension will not occur, fills
        // with zeroes.
        sampleSquidge = sampleSqueeze >> (16 - crushBits);

        // shift bits left again to regain the volume level.
        // fills with zeroes. paste into buffer sample +
        // sampleStep offset.
        samples[i] = sampleSquidge << (16 - crushBits);
        i++;
      }
    }
  }
}
