#include <assert.h>
#include <normalize.h>
#include <stddef.h>
#include <utf.h>

#include "helpers.h"

int
main() {
  // A handful of canonical compositions whose second code point is a starter, one
  // with a combining class of zero, rather than a combining mark. These are the
  // Indic vowel signs that make it wrong to guard the composition search on the
  // second being a mark; the guard has to admit them, so they are exercised here to
  // catch a version of that guard that would drop them.
  {
    // Bengali letter E followed by the sign AA composes to the sign O.
    assert(normalize_compose(0x9c7, 0x9be) == 0x9cb);

    // Oriya letter E followed by the sign AA composes to the sign O.
    assert(normalize_compose(0xb47, 0xb3e) == 0xb4b);

    // Tamil letter O followed by the sign AU length mark composes to the letter AU.
    assert(normalize_compose(0xb92, 0xbd7) == 0xb94);

    // The whole round trip holds through both forms as well, the decomposition being
    // two starters that must come back together.
    static const utf32_t composed[] = {0x9cb};
    static const utf32_t decomposed[] = {0x9c7, 0x9be};

    test_nfc(decomposed, 2, composed, 1);
    test_nfd(composed, 1, decomposed, 2);
  }
}
