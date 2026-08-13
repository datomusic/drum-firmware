#include "duo/audio/effect_custom_envelope.h"

#include "musin/audio/audio_output.h"
#include "musin/audio/dspinst.h"

extern "C" {
#include "hardware/sync.h"
#include "pico/platform.h"
}

namespace {
constexpr float SAMPLES_PER_MSEC =
    static_cast<float>(AudioOutput::SAMPLE_FREQUENCY) / 1000.0f;

int milliseconds2count(float milliseconds) {
  if (milliseconds < 0.0f) {
    milliseconds = 0.0f;
  }
  return static_cast<int>(milliseconds * SAMPLES_PER_MSEC);
}
} // namespace

namespace duo::audio {

void __time_critical_func(CustomEnvelope::fill_buffer)(
    ::AudioBlock &out_samples) {
  source_.fill_buffer(out_samples);

  int16_t *p = out_samples.begin();
  int16_t *end = out_samples.end();
  while (p < end) {
    // The envelope advances once per SUBDIVISION samples; the increment is
    // applied per sample, matching the original packed-word implementation.
    int v = env_.curVal;
    const int inc = env_.step();
    for (int i = 0; i < SUBDIVISION; ++i) {
      *p = static_cast<int16_t>(signed_multiply_32x16b(v, *p));
      ++p;
      v += inc;
    }
  }
}

void CustomEnvelope::updateEnv() {
  env_.adsr(milliseconds2count(attackMs), milliseconds2count(decayMs),
            sustainVal, milliseconds2count(releaseMs));
}

void CustomEnvelope::attack(int milliseconds) {
  attackMs = milliseconds;
  updateEnv();
}

void CustomEnvelope::decay(int milliseconds) {
  decayMs = milliseconds;
  updateEnv();
}

void CustomEnvelope::sustain(float level) {
  sustainVal = static_cast<int>(level * ENVELOPE_MAX);
  updateEnv();
}

void CustomEnvelope::release(int milliseconds) {
  releaseMs = milliseconds;
  updateEnv();
}

void CustomEnvelope::noteOn() {
  uint32_t saved = save_and_disable_interrupts();
  env_.on();
  restore_interrupts(saved);
}

void CustomEnvelope::noteOff() {
  uint32_t saved = save_and_disable_interrupts();
  env_.off();
  restore_interrupts(saved);
}

} // namespace duo::audio
