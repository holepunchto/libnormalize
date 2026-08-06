#include <assert.h>
#include <normalize.h>
#include <stddef.h>
#include <utf.h>

int
main() {
  // The combining class of a starter is zero, and of a mark is what the database
  // gives it.
  {
    assert(normalize_combining_class('a') == 0);
    assert(normalize_combining_class(0x301) == 230);
    assert(normalize_combining_class(0x323) == 220);

    // The virama that the IDNA ContextJ rules look for.
    assert(normalize_combining_class(0x94d) == 9);

    // Everything below the first mark is a starter without a lookup.
    for (utf32_t c = 0; c < NORMALIZE_FIRST_MARK; c++) {
      assert(normalize_combining_class(c) == 0);
    }
  }

  // Composing a pair gives the primary composite, and 0 for a pair that does not
  // compose.
  {
    assert(normalize_compose(0x65, 0x301) == 0xe9);
    assert(normalize_compose(0x1100, 0x1161) == 0xac00);
    assert(normalize_compose(0xac00, 0x11a8) == 0xac01);

    assert(normalize_compose(0x61, 0x62) == 0);
    assert(normalize_compose(0x65, 0x65) == 0);

    // An excluded composite is absent from the table.
    assert(normalize_compose(0x915, 0x93c) == 0);
  }

  // Decomposing a single code point never writes nothing, and never more than the
  // room it is promised.
  {
    utf32_t result[NORMALIZE_MAX_DECOMPOSITION];

    assert(normalize_decompose('a', result) == 1);
    assert(result[0] == 'a');

    assert(normalize_decompose(0xe9, result) == 2);

    size_t longest = 0;

    for (utf32_t c = 0; c <= 0x10ffff; c++) {
      size_t len = normalize_decompose(c, result);

      assert(len >= 1);
      assert(len <= NORMALIZE_MAX_DECOMPOSITION);

      if (len > longest) longest = len;
    }

    // The constant must be tight, or it is a bound taken from nowhere.
    assert(longest == NORMALIZE_MAX_DECOMPOSITION);
  }
}
