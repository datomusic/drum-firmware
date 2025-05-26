#ifndef TEST_SUPPORT_H_O674NK3Y
#define TEST_SUPPORT_H_O674NK3Y

#include <catch2/catch_test_macros.hpp>

#if STATIC_TESTS == 1

#define CONST_BODY(BODY)                                                                           \
  constexpr auto body = []() {                                                                     \
    BODY;                                                                                          \
    return 0;                                                                                      \
  };                                                                                               \
  constexpr const auto _ = body();                                                                 \
  body();

#undef REQUIRE
#define REQUIRE assert

#else

#define CONST_BODY(body) body

#endif

#endif /* end of include guard: TEST_SUPPORT_H_O674NK3Y */
