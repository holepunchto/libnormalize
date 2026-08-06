#include <assert.h>
#include <normalize.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Take the input as UTF-8 so that the fuzzer arrives at sequences of code points
  // rather than at the raw words that reading it as UTF-32 would give.
  if (!utf8_validate((const utf8_t *) data, size)) return 0;

  size_t len = utf32_length_from_utf8((const utf8_t *) data, size);

  size_t bound = normalize_max_length(len);

  if (bound == (size_t) -1) return 0;

  utf32_t *input = (utf32_t *) malloc(len * sizeof(utf32_t) + 1);
  utf32_t *nfd = (utf32_t *) malloc(bound * sizeof(utf32_t) + 1);
  utf32_t *nfc = (utf32_t *) malloc(bound * sizeof(utf32_t) + 1);

  if (input == NULL || nfd == NULL || nfc == NULL) goto done;

  len = utf8_convert_to_utf32((const utf8_t *) data, size, input);

  size_t nfd_len = normalize_nfd(input, len, nfd);
  size_t nfc_len = normalize_nfc(input, len, nfc);

  // The bound must hold, or either wrote past what a caller would have allocated.
  assert(nfd_len <= bound);
  assert(nfc_len <= bound);

  // Decomposing only ever lengthens, and composing only ever shortens what
  // decomposing produced.
  assert(nfd_len >= len);
  assert(nfc_len <= nfd_len);

  // Both forms are idempotent, which is the property that the whole of
  // normalization rests on.
  {
    size_t again_bound = normalize_max_length(nfd_len);

    if (again_bound != (size_t) -1) {
      utf32_t *again = (utf32_t *) malloc(again_bound * sizeof(utf32_t) + 1);

      if (again != NULL) {
        size_t again_len = normalize_nfd(nfd, nfd_len, again);

        assert(again_len == nfd_len);
        assert(memcmp(again, nfd, nfd_len * sizeof(utf32_t)) == 0);

        // Composing a decomposition gives the same as composing the input, which is
        // what makes the two forms agree on what is equivalent.
        again_len = normalize_nfc(nfd, nfd_len, again);

        assert(again_len == nfc_len);
        assert(memcmp(again, nfc, nfc_len * sizeof(utf32_t)) == 0);

        free(again);
      }
    }
  }

  {
    size_t again_bound = normalize_max_length(nfc_len);

    if (again_bound != (size_t) -1) {
      utf32_t *again = (utf32_t *) malloc(again_bound * sizeof(utf32_t) + 1);

      if (again != NULL) {
        size_t again_len = normalize_nfc(nfc, nfc_len, again);

        assert(again_len == nfc_len);
        assert(memcmp(again, nfc, nfc_len * sizeof(utf32_t)) == 0);

        // Decomposing a composition gives back the decomposition.
        again_len = normalize_nfd(nfc, nfc_len, again);

        assert(again_len == nfd_len);
        assert(memcmp(again, nfd, nfd_len * sizeof(utf32_t)) == 0);

        free(again);
      }
    }
  }

  // A decomposition is in canonical order, so no mark precedes one of a lower
  // non-zero class.
  for (size_t i = 1; i < nfd_len; i++) {
    uint8_t previous = normalize_combining_class(nfd[i - 1]);
    uint8_t current = normalize_combining_class(nfd[i]);

    if (current != 0 && previous != 0) assert(previous <= current);
  }

done:
  free(nfc);
  free(nfd);
  free(input);

  return 0;
}
