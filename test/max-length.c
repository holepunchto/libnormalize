#include <assert.h>
#include <normalize.h>
#include <stddef.h>
#include <stdint.h>

int
main() {
  // The bound allows for the longest decomposition of every code point.
  {
    assert(normalize_max_length(0) == 0);
    assert(normalize_max_length(1) == NORMALIZE_MAX_DECOMPOSITION);
    assert(normalize_max_length(63) == 63 * NORMALIZE_MAX_DECOMPOSITION);
  }

  // A length that cannot be scaled without overflowing is rejected rather than
  // wrapping to a bound too small to normalize within.
  {
    assert(normalize_max_length(SIZE_MAX) == (size_t) -1);

    size_t fits = SIZE_MAX / NORMALIZE_MAX_DECOMPOSITION;

    size_t bound = normalize_max_length(fits);

    assert(bound != (size_t) -1);

    // A bound that had wrapped would come back smaller than the length it was asked
    // about, which is the failure the guard exists to prevent.
    assert(bound >= fits);

    assert(normalize_max_length(fits + 1) == (size_t) -1);
  }
}
