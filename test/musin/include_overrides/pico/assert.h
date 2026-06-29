#ifndef TEST_MUSIN_INCLUDE_OVERRIDES_MOCK_PICO_ASSERT_H_
#define TEST_MUSIN_INCLUDE_OVERRIDES_MOCK_PICO_ASSERT_H_

#include <cassert>

#ifndef hard_assert
#define hard_assert(...) assert(__VA_ARGS__)
#endif

#endif // TEST_MUSIN_INCLUDE_OVERRIDES_MOCK_PICO_ASSERT_H_
