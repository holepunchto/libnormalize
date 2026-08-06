#include <normalize.h>
#include <stddef.h>
#include <utf.h>

int
main() {
  // A domain label of the sort that the IDNA path normalizes, given decomposed so
  // that both halves of composing have work to do.
  static const utf32_t input[] = {
    0x6d, 0x75, 0x308, 0x6e, 0x63, 0x68, 0x65, 0x6e
  };

  size_t len = sizeof(input) / sizeof(utf32_t);

  utf32_t result[64];

  for (size_t i = 0; i < 1000000; i++) {
    normalize_nfc(input, len, result);
  }
}
