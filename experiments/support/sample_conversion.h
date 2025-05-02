#ifndef SAMPLE_CONVERSION_H_40B6KHDD
#define SAMPLE_CONVERSION_H_40B6KHDD

#include <array>
#include <cstddef>
#include <cstdint>

template <size_t N>
consteval std::array<int16_t, N / 2> int16FromBytes(const unsigned char input[N]) {
  static_assert(N % 2 == 0, "Input size must be even to form 16-bit integers.");
  std::array<int16_t, N / 2> result{};

  for (size_t i = 0; i < N / 2; ++i) {
    unsigned char tmp[2] = {input[2 * i], input[2 * i + 1]};
    result[i] = std::bit_cast<int16_t>(tmp);
  }

  return result;
}

#endif /* end of include guard: SAMPLE_CONVERSION_H_40B6KHDD */
