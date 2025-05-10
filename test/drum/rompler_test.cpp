#include "drum/rompler.h"
#include "test_support.h"

TEST_CASE("Rompler runs") {
  CONST_BODY(({
    SampleBank samples;
    Rompler romp(samples);
    romp.get_voice(3).play();

    REQUIRE(1 == 1);
  }));
}
