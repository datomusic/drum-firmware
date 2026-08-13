#ifndef DUO_AUDIO_EFFECT_CUSTOM_ENVELOPE_H
#define DUO_AUDIO_EFFECT_CUSTOM_ENVELOPE_H

#include "duo/audio/envelope.h"
#include "musin/audio/buffer_source.h"

namespace duo::audio {

/**
 * @brief Linear ADSR amplitude envelope, ported from the DUO's
 * AudioEffectCustomEnvelope (shared/duo) to musin's pull model.
 *
 * Wraps an upstream BufferSource and scales its output by the envelope.
 * fill_buffer runs in the I2S DMA interrupt and must stay RAM-resident.
 */
class CustomEnvelope : public ::BufferSource {
  static constexpr int ENVELOPE_MAX = 0x10000;
  static constexpr int SUBDIVISION = 8;

public:
  explicit CustomEnvelope(::BufferSource &source)
      : source_(source), env_(ENVELOPE_MAX, SUBDIVISION) {
  }

  void noteOn();
  void noteOff();
  void attack(int milliseconds);
  void decay(int milliseconds);
  void sustain(float level);
  void release(int milliseconds);

  void fill_buffer(::AudioBlock &out_samples) override;

private:
  void updateEnv();

  ::BufferSource &source_;
  LinearEnvelope env_;
  int attackMs = 0;
  int decayMs = 0;
  int sustainVal = 0;
  int releaseMs = 0;
};

} // namespace duo::audio

#endif /* DUO_AUDIO_EFFECT_CUSTOM_ENVELOPE_H */
