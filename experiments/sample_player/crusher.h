#ifndef CRUSHER_H_4BACOXIO
#define CRUSHER_H_4BACOXIO

#include "musin/audio/buffer_source.h"
#include "musin/audio/teensy/effect_bitcrusher.h"

struct Crusher : BufferSource {
  Crusher(BufferSource &source) : source(source) {
  }

  uint32_t fill_buffer(int16_t *out_samples) {
    const auto count = source.fill_buffer(out_samples);
    crusher.update(out_samples, count);
    return count;
  }

  void bits(const uint8_t b) {
    crusher.bits(b);
  }

  void sampleRate(const float hz) {
    crusher.sampleRate(hz);
  }

private:
  BufferSource &source;
  AudioEffectBitcrusher crusher;
};

#endif /* end of include guard: CRUSHER_H_4BACOXIO */
