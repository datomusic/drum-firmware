#include <cassert> // For assert when STATIC_TESTS is enabled
#include <catch2/catch_test_macros.hpp>

#include "musin/audio/block.h" // Include AudioBlock definition
#include "musin/audio/memory_reader.h"

// While working on tests, disable constexpr testing to get proper assertions.
#define STATIC_TESTS 0 // Set to 1 to enable constexpr tests with assert

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

TEST_CASE("MemorySampleReader streams samples") {
  CONST_BODY(({
    const int16_t samples[2] = {123, 33};
    // Use the namespaced version
    Musin::MemorySampleReader decoder(samples, 2);

    // Assuming AudioBlock is still global based on previous info
    AudioBlock block;
    auto block_data = block.begin();

    auto count = decoder.read_samples(block);
    REQUIRE(count == 2);
    REQUIRE(block_data[0] == 123);
    REQUIRE(block_data[1] == 33);

    count = decoder.read_samples(block);
    REQUIRE(count == 0);

    decoder.reset();
    count = decoder.read_samples(block);
    REQUIRE(count == 2);
  }));
}
