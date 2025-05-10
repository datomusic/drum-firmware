#include "drum/rompler.h"
#include "test_support.h"

TEST_CASE("Rompler runs") {
  CONST_BODY(({
    SampleBank samples;
    Rompler romp(samples);
    for (int i = 0; i < Rompler::VoiceCount; ++i) {
      romp.get_voice(i).play();
    }

    REQUIRE(1 == 1);
  }));
}
