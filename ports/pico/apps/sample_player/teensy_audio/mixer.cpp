#include "mixer.h"
#include "dspinst.h"
#include <stdint.h>

static int16_t temp_buffer[AUDIO_BLOCK_SAMPLES];

uint32_t __not_in_flash_func(AudioMixer4::fill_buffer)(int16_t *out_samples) {
  for (int sample_index = 0; sample_index < AUDIO_BLOCK_SAMPLES;
       ++sample_index) {
    out_samples[sample_index] = 0;
  }

  for (int channel = 0; channel < source_count; ++channel) {
    sources[channel]->fill_buffer(temp_buffer);
    for (int sample_index = 0; sample_index < AUDIO_BLOCK_SAMPLES;
         ++sample_index) {
      const int16_t multiplier = multipliers[channel];

      const int32_t value = out_samples[sample_index] +
                            ((temp_buffer[sample_index] * multiplier) >> 8);
      out_samples[sample_index] = signed_saturate_rshift(value, 16, 0);
    }
  }

  return AUDIO_BLOCK_SAMPLES;
}
