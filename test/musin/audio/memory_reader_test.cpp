#include <catch2/catch_test_macros.hpp>

#include "musin/audio/memory_reader.h"

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

// #include <array>
// #include <cstdint>

/*
// Converts an array of unsigned char to an array of int16_t at compile time.
template <size_t N>
consteval std::array<int16_t, N / 2> convertToInt16(const unsigned char input[N]) {
  static_assert(N % 2 == 0, "Input size must be even to form 16-bit integers.");
  std::array<int16_t, N / 2> result{};

  for (size_t i = 0; i < N / 2; ++i) {
    result[i] = static_cast<int16_t>((static_cast<int16_t>(input[2 * i]) << 8) | input[2 * i + 1]);
  }

  return result;
}
*/

TEST_CASE("MemorySampleReader streams samples") {
  CONST_BODY(({
    /*
    // Example input array of unsigned char
    constexpr unsigned char input[] = {0x01, 0x01, 0x03, 0x04, 0xFF, 0xFE, 0x00, 0x01};

    // Convert at compile time
    constexpr auto convertedArray = convertToInt16<8>(input);
    REQUIRE(convertedArray[0] == 257);
    */

    const int16_t samples[2] = {123, 33};
    MemorySampleReader decoder(samples, 2);

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
