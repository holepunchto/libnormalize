// Checks the normalizer against NormalizationTest.txt, the conformance test that
// the Unicode Character Database ships.
//
// Usage:
//
//   conformance <path to NormalizationTest.txt>
//
// https://www.unicode.org/reports/tr15/#Conformance_Testing

#include <assert.h>
#include <normalize.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

#define MAX_SEQUENCE 64
#define MAX_RESULT   (MAX_SEQUENCE * NORMALIZE_MAX_DECOMPOSITION)

typedef struct {
  utf32_t data[MAX_SEQUENCE];
  size_t len;
} sequence_t;

static size_t tested = 0;
static size_t failed = 0;

// Parses a space separated column of hex code points.
static bool
parse(const char *field, sequence_t *sequence) {
  sequence->len = 0;

  const char *p = field;

  while (*p != '\0') {
    while (*p == ' ')
      p++;

    if (*p == '\0') break;

    char *end;

    unsigned long c = strtoul(p, &end, 16);

    if (end == p) return false;
    if (sequence->len == MAX_SEQUENCE) return false;

    sequence->data[sequence->len++] = (utf32_t) c;

    p = end;
  }

  return true;
}

static void
print(const char *label, const utf32_t *data, size_t len) {
  printf("  %s:", label);

  for (size_t i = 0; i < len; i++)
    printf(" %04X", data[i]);

  printf("\n");
}

// Checks that normalizing `input` in the given form gives `expected`.
static void
check(const char *form, const sequence_t *input, const sequence_t *expected, size_t line, bool composed) {
  utf32_t result[MAX_RESULT];

  size_t len = composed
                 ? normalize_nfc(input->data, input->len, result)
                 : normalize_nfd(input->data, input->len, result);

  tested++;

  if (len == expected->len && memcmp(result, expected->data, len * sizeof(utf32_t)) == 0) {
    return;
  }

  failed++;

  printf("line %zu: %s mismatch\n", line, form);
  print("input", input->data, input->len);
  print("expected", expected->data, expected->len);
  print("actual", result, len);
}

int
main(int argc, char *argv[]) {
  assert(argc == 2);

  FILE *file = fopen(argv[1], "r");
  assert(file != NULL);

  // The code points that appear on their own in Part 1, which are the only ones
  // that need not normalize to themselves.
  bool *listed = (bool *) calloc(0x110000, sizeof(bool));
  assert(listed != NULL);

  char line[4096];
  size_t number = 0;
  int part = -1;

  while (fgets(line, sizeof(line), file) != NULL) {
    number++;

    if (line[0] == '@') {
      // A part marker, such as `@Part1`.
      part = atoi(&line[5]);
      continue;
    }

    // Strip the comment, which for a data line follows the last column.
    char *hash = strchr(line, '#');
    if (hash != NULL) *hash = '\0';

    if (strchr(line, ';') == NULL) continue;

    sequence_t columns[5];
    size_t count = 0;

    char *rest = line;

    for (char *field = strsep(&rest, ";"); field != NULL && count < 5; field = strsep(&rest, ";")) {
      if (!parse(field, &columns[count])) break;

      count++;
    }

    if (count < 5) continue;

    const sequence_t *source = &columns[0];
    const sequence_t *nfc = &columns[1];
    const sequence_t *nfd = &columns[2];
    const sequence_t *nfkc = &columns[3];
    const sequence_t *nfkd = &columns[4];

    // The invariants that the conformance test states, less the ones over the
    // compatibility forms, which are not normalization forms this implements.
    check("NFC(source)", source, nfc, number, true);
    check("NFC(NFC)", nfc, nfc, number, true);
    check("NFC(NFD)", nfd, nfc, number, true);
    check("NFC(NFKC)", nfkc, nfkc, number, true);
    check("NFC(NFKD)", nfkd, nfkc, number, true);

    check("NFD(source)", source, nfd, number, false);
    check("NFD(NFC)", nfc, nfd, number, false);
    check("NFD(NFD)", nfd, nfd, number, false);
    check("NFD(NFKC)", nfkc, nfkd, number, false);
    check("NFD(NFKD)", nfkd, nfkd, number, false);

    if (part == 1 && source->len == 1) listed[source->data[0]] = true;
  }

  fclose(file);

  // Every code point that Part 1 does not list must be left alone by both forms.
  size_t identity = 0;
  size_t identity_failed = 0;

  for (utf32_t c = 0; c <= 0x10ffff; c++) {
    if (listed[c]) continue;

    utf32_t result[NORMALIZE_MAX_DECOMPOSITION];

    identity++;

    size_t len = normalize_nfc(&c, 1, result);

    if (len != 1 || result[0] != c) {
      if (identity_failed++ == 0) printf("NFC(%04X) is not %04X\n", c, c);
      continue;
    }

    len = normalize_nfd(&c, 1, result);

    if (len != 1 || result[0] != c) {
      if (identity_failed++ == 0) printf("NFD(%04X) is not %04X\n", c, c);
    }
  }

  printf("%zu passed, %zu failed\n", tested - failed, failed);
  printf("%zu code points unlisted by part 1 left alone, %zu not\n", identity - identity_failed, identity_failed);

  free(listed);

  assert(failed == 0);
  assert(identity_failed == 0);
}
