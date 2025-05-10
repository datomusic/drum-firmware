#ifndef ROMPLER_H_MXAFSKQ3
#define ROMPLER_H_MXAFSKQ3

#include "etl/array.h"
#include "sample_bank/sample_bank.h"

namespace rompler {

struct VoiceControls {
  constexpr virtual void play() = 0;
};

struct Voice : VoiceControls {
  constexpr Voice(SampleBank &bank) : bank(bank) {
  }

  constexpr void play() override {
  }

  SampleBank &bank;
};

} // namespace rompler

struct Rompler {
  static const uint8_t VoiceCount = 4;

  constexpr Rompler(SampleBank &bank) : bank(bank) {
  }

  constexpr rompler::VoiceControls &get_voice(const unsigned index) {
    return voices[index];
  }

private:
  SampleBank bank;

  typedef rompler::Voice Voice;
  etl::array<rompler::Voice, 4> voices = {Voice(bank), Voice(bank), Voice(bank), Voice(bank)};
};

#endif /* end of include guard: ROMPLER_H_MXAFSKQ3 */
