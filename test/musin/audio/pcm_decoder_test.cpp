#include <catch2/catch_test_macros.hpp>

#include "musin/audio/pcm_decoder.h"

using std::byte;

// While working on tests, disable constexpr testing to get proper assertions.
#define STATIC_TESTS 1

#if STATIC_TESTS == 0
#define CONST_BODY
#else
#define CONST_BODY(BODY)                                                                           \
  constexpr auto body = []() {                                                                     \
    BODY;                                                                                          \
    return 0;                                                                                      \
  };                                                                                               \
  constexpr const auto _ = body();                                                                 \
  body();
#undef REQUIRE
#define REQUIRE assert
#endif


TEST_CASE("PcmDecoder decodes single sample") {
  CONST_BODY(({
    const byte high_bits[2] = {byte(0), byte(1)};
    PcmDecoder decoder(high_bits, 2);
    decoder.reset();

    AudioBlock block;
    auto block_data = block.begin();

    auto count = decoder.read_samples(block);
    REQUIRE(count == 1);
    REQUIRE(block_data[0] == 256);

    const byte low_bits[2] = {byte(1), byte(0)};
    decoder.set_source(low_bits, 2);
    count = decoder.read_samples(block);
    REQUIRE(count == 1);
    REQUIRE(block_data[0] == 1);

    // Don't memory corrupt with uneven sample count
    decoder.set_source(low_bits, 3);
    count = decoder.read_samples(block);
    REQUIRE(count == 1);

    // Don't memory corrupt with 0 samples
    decoder.set_source(low_bits, 0);
    count = decoder.read_samples(block);
    REQUIRE(count == 0);
  }));
}
