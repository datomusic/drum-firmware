#include "mixer.h"
#include "dspinst.h"
#include <cstddef>

static AudioBlock temp_buffer;

void AudioMixer4::fill_buffer(AudioBlock &out_samples) {
  for (size_t sample_index = 0; sample_index < out_samples.size();
       ++sample_index) {
    out_samples[sample_index] = 0;
  }

  for (unsigned int channel = 0; channel < sources.size(); ++channel) {
    sources[channel]->fill_buffer(temp_buffer);
    for (size_t sample_index = 0; sample_index < out_samples.size();
         ++sample_index) {
      const int16_t multiplier = multipliers[channel];

      const int32_t value = out_samples[sample_index] +
                            ((temp_buffer[sample_index] * multiplier) >> 8);
      out_samples[sample_index] = signed_saturate_rshift(value, 16, 0);
    }
  }
}
