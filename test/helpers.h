#include <assert.h>
#include <normalize.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <utf.h>

// Normalizes `input` into the given form and asserts that it comes out as
// `expected`.
static inline void
test_normalize(bool composed, const utf32_t *input, size_t len, const utf32_t *expected, size_t expected_len) {
  utf32_t result[64];

  assert(normalize_max_length(len) <= 64);

  size_t out = composed
                 ? normalize_nfc(input, len, result)
                 : normalize_nfd(input, len, result);

  assert(out == expected_len);
  assert(memcmp(result, expected, out * sizeof(utf32_t)) == 0);
}

#define test_nfc(input, len, expected, expected_len) \
  test_normalize(true, input, len, expected, expected_len)

#define test_nfd(input, len, expected, expected_len) \
  test_normalize(false, input, len, expected, expected_len)
