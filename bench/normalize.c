#include <normalize.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <utf.h>

// The number of times each case is normalized in one timed run.
#define ITERATIONS 1000000

// The number of timed runs to take, the best of which is reported. Timing the
// same work several times and keeping the fastest run rejects the noise that a
// busy machine adds without letting it flatter the result.
#define RUNS 7

typedef struct {
  const char *name;
  const utf32_t *input;
  size_t len;
} bench_case_t;

// Prevents the compiler from seeing that the result of normalizing goes unused
// and eliding the whole benchmark.
static volatile size_t sink;

static double
run_case(const bench_case_t *bench) {
  utf32_t result[64];

  double best = 1e30;

  for (size_t run = 0; run < RUNS; run++) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < ITERATIONS; i++) {
      sink = normalize_nfc(bench->input, bench->len, result);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    double per = ns / ITERATIONS;

    if (per < best) best = per;
  }

  return best;
}

int
main() {
  // An already-NFC ASCII label, the common case for a plain domain.
  static const utf32_t ascii[] = {
    0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65 // example
  };

  // An already-NFC CJK label, which decomposes to itself but pays a composition
  // lookup per code point today.
  static const utf32_t cjk[] = {
    0x65e5, 0x672c, 0x8a9e // 日本語
  };

  // An already-NFC Latin label with a precomposed accent.
  static const utf32_t latin[] = {
    0x6d, 0xfc, 0x6e, 0x63, 0x68, 0x65, 0x6e // münchen
  };

  // An already-NFC Hangul label, decomposed and recomposed by arithmetic.
  static const utf32_t hangul[] = {
    0xd55c, 0xad6d, 0xc5b4 // 한국어
  };

  // The decomposed form of the Latin label, so that both halves of composing
  // have real work to do. This is the original benchmark input.
  static const utf32_t decomposed[] = {
    0x6d, 0x75, 0x308, 0x6e, 0x63, 0x68, 0x65, 0x6e // münchen, u + diaeresis
  };

  // A pathological run of combining marks on a single starter, which reorders
  // and composes as far as it can.
  static const utf32_t marks[] = {
    0x61, 0x300, 0x301, 0x302, 0x303, 0x304, 0x305, 0x306,
    0x307, 0x308, 0x309, 0x30a, 0x30b, 0x30c, 0x30d, 0x30e
  };

  const bench_case_t cases[] = {
    {"ascii", ascii, sizeof(ascii) / sizeof(utf32_t)},
    {"cjk", cjk, sizeof(cjk) / sizeof(utf32_t)},
    {"latin", latin, sizeof(latin) / sizeof(utf32_t)},
    {"hangul", hangul, sizeof(hangul) / sizeof(utf32_t)},
    {"decomposed", decomposed, sizeof(decomposed) / sizeof(utf32_t)},
    {"marks", marks, sizeof(marks) / sizeof(utf32_t)},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    double per = run_case(&cases[i]);

    printf("%-12s %7.2f ns/op\n", cases[i].name, per);
  }
}
