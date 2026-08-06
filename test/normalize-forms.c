#include <assert.h>
#include <normalize.h>
#include <stddef.h>
#include <utf.h>

#include "helpers.h"

int
main() {
  // Nothing at all normalizes to nothing, rather than reading past the input.
  {
    utf32_t result[1];

    assert(normalize_nfc(NULL, 0, result) == 0);
    assert(normalize_nfd(NULL, 0, result) == 0);
  }

  // A precomposed character and its decomposition are the same under both forms.
  {
    static const utf32_t composed[] = {0xe9};          // é
    static const utf32_t decomposed[] = {0x65, 0x301}; // e + combining acute

    test_nfc(composed, 1, composed, 1);
    test_nfc(decomposed, 2, composed, 1);
    test_nfd(composed, 1, decomposed, 2);
    test_nfd(decomposed, 2, decomposed, 2);
  }

  // Canonical ordering sorts combining marks by class, and composition then applies
  // to the reordered sequence.
  {
    // a + combining dot below (220) + combining acute (230) is already ordered.
    static const utf32_t ordered[] = {0x61, 0x323, 0x301};
    // The same marks the other way around must come back ordered.
    static const utf32_t unordered[] = {0x61, 0x301, 0x323};

    // U+1EA1 is a with dot below, which then takes the acute.
    static const utf32_t nfc[] = {0x1ea1, 0x301};

    test_nfd(unordered, 3, ordered, 3);
    test_nfd(ordered, 3, ordered, 3);
    test_nfc(unordered, 3, nfc, 2);
    test_nfc(ordered, 3, nfc, 2);
  }

  // Marks of the same combining class are left in the order they were given, the
  // ordering being stable.
  {
    // Two marks both of class 230.
    static const utf32_t marks[] = {0x61, 0x301, 0x302};

    test_nfd(marks, 3, marks, 3);
  }

  // A Hangul syllable decomposes by arithmetic rather than by the tables, both with
  // and without a trailing consonant.
  {
    static const utf32_t lv[] = {0xac00};
    static const utf32_t lv_parts[] = {0x1100, 0x1161};

    static const utf32_t lvt[] = {0xac01};
    static const utf32_t lvt_parts[] = {0x1100, 0x1161, 0x11a8};

    test_nfd(lv, 1, lv_parts, 2);
    test_nfc(lv_parts, 2, lv, 1);

    test_nfd(lvt, 1, lvt_parts, 3);
    test_nfc(lvt_parts, 3, lvt, 1);

    // The last syllable of the block, to catch an off-by-one in the arithmetic.
    static const utf32_t last[] = {0xd7a3};
    static const utf32_t last_parts[] = {0x1112, 0x1175, 0x11c2};

    test_nfd(last, 1, last_parts, 3);
    test_nfc(last_parts, 3, last, 1);
  }

  // A code point on the composition exclusion list decomposes but does not come
  // back together. U+0958 is such a case.
  {
    static const utf32_t excluded[] = {0x958};
    static const utf32_t parts[] = {0x915, 0x93c};

    test_nfd(excluded, 1, parts, 2);
    test_nfc(excluded, 1, parts, 2);
    test_nfc(parts, 2, parts, 2);
  }

  // A singleton decomposition maps to one code point and never composes back.
  // U+2126 OHM SIGN is one, decomposing to U+03A9 GREEK CAPITAL LETTER OMEGA.
  {
    static const utf32_t ohm[] = {0x2126};
    static const utf32_t omega[] = {0x3a9};

    test_nfd(ohm, 1, omega, 1);
    test_nfc(ohm, 1, omega, 1);
  }

  // A decomposition is recursive, so a code point whose decomposition itself
  // decomposes comes fully apart in one step.
  {
    // U+1E14 is E with macron and grave, decomposing to E + macron + grave.
    static const utf32_t input[] = {0x1e14};
    static const utf32_t parts[] = {0x45, 0x304, 0x300};

    test_nfd(input, 1, parts, 3);
    test_nfc(input, 1, input, 1);
  }

  // A sequence beginning with a combining mark has no starter to compose with, so
  // the mark is left where it is.
  {
    static const utf32_t leading[] = {0x301, 0x61};

    test_nfc(leading, 2, leading, 2);
    test_nfd(leading, 2, leading, 2);
  }

  // A blocked mark does not compose with the starter it is separated from. Here the
  // acute is blocked from the a by a mark of the same class.
  {
    static const utf32_t blocked[] = {0x61, 0x301, 0x301};

    utf32_t result[64];

    size_t out = normalize_nfc(blocked, 3, result);

    // The first acute composes onto the a, the second cannot.
    assert(out == 2);
    assert(result[0] == 0xe1);
    assert(result[1] == 0x301);
  }
}
