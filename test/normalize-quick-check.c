#include <assert.h>
#include <normalize.h>
#include <stddef.h>
#include <utf.h>

#include "helpers.h"

int
main() {
  // A string already in Normalization Form C comes back unchanged, which is the
  // quick check taking the input as it stands rather than composing it. The cases
  // span the scripts that reach the check by a different route: ASCII below the
  // first mark, CJK that decomposes to itself, a precomposed Latin accent, and a
  // Hangul syllable. That the output equals the input is what the quick check has to
  // preserve.
  {
    static const utf32_t ascii[] = {0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65};
    static const utf32_t cjk[] = {0x65e5, 0x672c, 0x8a9e};
    static const utf32_t latin[] = {0x6d, 0xfc, 0x6e, 0x63, 0x68, 0x65, 0x6e};
    static const utf32_t hangul[] = {0xd55c, 0xad6d, 0xc5b4};

    test_nfc(ascii, 7, ascii, 7);
    test_nfc(cjk, 3, cjk, 3);
    test_nfc(latin, 7, latin, 7);
    test_nfc(hangul, 3, hangul, 3);
  }

  // An input that is not already normalized must not be taken for normalized by the
  // quick check. A decomposed accent has to be composed, and a pair of marks out of
  // canonical order has to be reordered, neither of which the quick check may pass.
  {
    static const utf32_t decomposed[] = {0x6d, 0x75, 0x308, 0x6e}; // mün, decomposed
    static const utf32_t composed[] = {0x6d, 0xfc, 0x6e};         // mün, composed

    test_nfc(decomposed, 4, composed, 3);

    // a + acute (230) + dot below (220), which reorders and then composes.
    static const utf32_t unordered[] = {0x61, 0x301, 0x323};
    static const utf32_t nfc[] = {0x1ea1, 0x301}; // a with dot below, then acute

    test_nfc(unordered, 3, nfc, 2);
  }
}
