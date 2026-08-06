# libnormalize

Unicode normalization in C, as specified by [UAX #15](https://www.unicode.org/reports/tr15). Provides Normalization Forms C and D over code points, allocating nothing itself, the length of a result being bounded ahead of writing it.

## Usage

```c
#include <normalize.h>
#include <utf.h>

// "mu" followed by a combining diaeresis.
const utf32_t input[] = {0x6d, 0x75, 0x308};

size_t len = sizeof(input) / sizeof(utf32_t);

utf32_t result[normalize_max_length(3)];

size_t result_len = normalize_nfc(input, len, result);
```

## API

See [`include/normalize.h`](include/normalize.h) for the public API.

Only the canonical forms are provided. The compatibility forms, NFKC and NFKD, discard distinctions that are not safe to discard for every purpose, and nothing this was written for needs them.

Normalization operates on code points, so text held as UTF-8 or UTF-16 is converted with [libutf](https://github.com/holepunchto/libutf) first.

## License

Apache-2.0
