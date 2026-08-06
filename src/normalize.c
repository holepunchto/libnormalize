#include <normalize/tables.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utf.h>

#include "../include/normalize.h"

/**
 * The constants of the algorithmic Hangul syllable decomposition, which is given by
 * arithmetic rather than by the tables.
 *
 * https://www.unicode.org/versions/latest/core-spec/chapter-3/#G56669
 */
enum {
  normalize__hangul_s_base = 0xac00,
  normalize__hangul_l_base = 0x1100,
  normalize__hangul_v_base = 0x1161,
  normalize__hangul_t_base = 0x11a7,
  normalize__hangul_l_count = 19,
  normalize__hangul_v_count = 21,
  normalize__hangul_t_count = 28,
  normalize__hangul_n_count = normalize__hangul_v_count * normalize__hangul_t_count,
  normalize__hangul_s_count = normalize__hangul_l_count * normalize__hangul_n_count,
};

/**
 * Looks up the value that a sorted, gap-free list of ranges gives a code point,
 * which is the value of the last range that starts at or below it. The search is
 * narrowed down to the block that the code point belongs to.
 */
static inline uint8_t
normalize__range_value(const normalize_range_t *ranges, const uint16_t *indexes, utf32_t c) {
  size_t block = c >> NORMALIZE_BLOCK_SHIFT;

  size_t lo = indexes[block], hi = indexes[block + 1];

  while (lo < hi) {
    size_t mid = lo + (hi - lo + 1) / 2;

    if (ranges[mid] >> 8 <= c) lo = mid;
    else hi = mid - 1;
  }

  return ranges[lo] & 0xff;
}

uint8_t
normalize_combining_class(utf32_t c) {
  if (c < NORMALIZE_FIRST_MARK) return 0;

  return normalize__range_value(
    normalize__combining_class_ranges,
    normalize__combining_class_blocks,
    c
  );
}

/**
 * The NFC_Quick_Check value of a code point, which is `Yes` for everything below
 * the first mark without a lookup, the same shortcut the combining class takes.
 *
 * https://www.unicode.org/reports/tr44/#NFC_QC
 */
static inline uint8_t
normalize__quick_check(utf32_t c) {
  if (c < NORMALIZE_FIRST_MARK) return NORMALIZE_QC_YES;

  return normalize__range_value(
    normalize__quick_check_ranges,
    normalize__quick_check_blocks,
    c
  );
}

size_t
normalize_max_length(size_t len) {
  if (len > SIZE_MAX / NORMALIZE_MAX_DECOMPOSITION) return (size_t) -1;

  return len * NORMALIZE_MAX_DECOMPOSITION;
}

size_t
normalize_decompose(utf32_t c, utf32_t *result) {
  if (c >= normalize__hangul_s_base && c < normalize__hangul_s_base + normalize__hangul_s_count) {
    uint32_t index = c - normalize__hangul_s_base;

    size_t len = 0;

    result[len++] = normalize__hangul_l_base + index / normalize__hangul_n_count;

    result[len++] = normalize__hangul_v_base + (index % normalize__hangul_n_count) / normalize__hangul_t_count;

    uint32_t t = index % normalize__hangul_t_count;

    if (t != 0) result[len++] = normalize__hangul_t_base + t;

    return len;
  }

  if (c < NORMALIZE_FIRST_DECOMPOSABLE) {
    result[0] = c;

    return 1;
  }

  size_t block = c >> NORMALIZE_BLOCK_SHIFT;

  size_t lo = normalize__decomposition_blocks[block], hi = normalize__decomposition_blocks[block + 1];

  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;

    const normalize__decomposition_t *entry = &normalize__decompositions[mid];

    if (entry->code_point < c) {
      lo = mid + 1;
    } else if (entry->code_point > c) {
      hi = mid;
    } else {
      size_t len = entry->decomposition >> 24;

      memcpy(
        result,
        &normalize__decomposition_data[entry->decomposition & 0xffffff],
        len * sizeof(utf32_t)
      );

      return len;
    }
  }

  result[0] = c;

  return 1;
}

/**
 * Searches the composition table for the primary composite of a pair, the two
 * having already been found to be neither a Hangul pair nor ruled out by the quick
 * check. This is kept out of `normalize_compose()` so that the cheap tests ahead of
 * it stay small enough to inline into the recompose loop, the search itself being
 * reached on only the code points that can actually compose.
 */
static utf32_t
normalize__compose_table(utf32_t first, utf32_t second) {
  size_t lo = 0, hi = NORMALIZE_COMPOSITIONS;

  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;

    const normalize__composition_t *entry = &normalize__compositions[mid];

    if (entry->first != first) {
      if (entry->first < first) lo = mid + 1;
      else hi = mid;
    } else if (entry->second != second) {
      if (entry->second < second) lo = mid + 1;
      else hi = mid;
    } else {
      return entry->composite;
    }
  }

  return 0;
}

utf32_t
normalize_compose(utf32_t first, utf32_t second) {
  // The second of the two code points of a composition is always a combining mark,
  // or else a Hangul vowel or trailing consonant, all of which sit above the first
  // combining mark.
  if (second < NORMALIZE_FIRST_MARK) return 0;

  if (
    first >= normalize__hangul_l_base &&
    first < normalize__hangul_l_base + normalize__hangul_l_count &&
    second >= normalize__hangul_v_base &&
    second < normalize__hangul_v_base + normalize__hangul_v_count
  ) {
    return normalize__hangul_s_base +
           ((first - normalize__hangul_l_base) * normalize__hangul_v_count + (second - normalize__hangul_v_base)) * normalize__hangul_t_count;
  }

  if (
    first >= normalize__hangul_s_base &&
    first < normalize__hangul_s_base + normalize__hangul_s_count &&
    (first - normalize__hangul_s_base) % normalize__hangul_t_count == 0 &&
    second > normalize__hangul_t_base &&
    second < normalize__hangul_t_base + normalize__hangul_t_count
  ) {
    return first + (second - normalize__hangul_t_base);
  }

  // Only a code point that takes part in a composition as the second of its two can
  // be found in the table, so any other need not be looked for. This is the common
  // case for text that does not compose, such as CJK, which would otherwise pay for a
  // full search of the table that only ever returns zero. The bitset is flat and so
  // answers in a single load, small enough to keep this test on the inlined path.
  if (second > NORMALIZE_MAX_COMPOSITION_SECOND) return 0;

  if (!(normalize__composition_second[second >> 3] & (1u << (second & 7)))) return 0;

  return normalize__compose_table(first, second);
}

/**
 * Puts a canonical decomposition into canonical order in place, by moving each
 * combining mark ahead of any preceding mark of a higher combining class. This is
 * all that Normalization Form D calls for beyond decomposing, and the first of the
 * two things that Form C does.
 *
 * https://www.unicode.org/reports/tr15/#Canonical_Ordering_Algorithm
 */
static void
normalize__reorder(utf32_t *data, size_t len) {
  for (size_t i = 1; i < len; i++) {
    uint8_t combining_class = normalize_combining_class(data[i]);

    if (combining_class == 0) continue;

    for (size_t j = i; j > 0 && normalize_combining_class(data[j - 1]) > combining_class; j--) {
      utf32_t c = data[j - 1];

      data[j - 1] = data[j];
      data[j] = c;
    }
  }
}

size_t
normalize_recompose(utf32_t *data, size_t len) {
  if (len == 0) return 0;

  normalize__reorder(data, len);

  // Compose each code point with the last starter, unless a code point of the same
  // or a higher combining class stands between the two and blocks it.
  size_t starter = 0, out = 1;

  uint32_t last = normalize_combining_class(data[0]);

  // A sequence beginning with a combining mark has no starter to compose with.
  if (last != 0) last = 0xff;

  for (size_t i = 1; i < len; i++) {
    utf32_t c = data[i];

    uint8_t combining_class = normalize_combining_class(c);

    // A code point that a mark of the same or a higher class stands between and its
    // starter is blocked from composing with it, so the composition need not even be
    // looked for. Testing the cheap combining class first keeps the table search off
    // the path for every blocked mark.
    utf32_t composite = 0;

    if (last == 0 || last < combining_class) {
      composite = normalize_compose(data[starter], c);
    }

    if (composite != 0) {
      data[starter] = composite;
    } else {
      if (combining_class == 0) starter = out;

      last = combining_class;

      data[out++] = c;
    }
  }

  return out;
}

size_t
normalize_nfd(const utf32_t *data, size_t len, utf32_t *result) {
  size_t out = 0;

  for (size_t i = 0; i < len; i++) {
    out += normalize_decompose(data[i], &result[out]);
  }

  normalize__reorder(result, out);

  return out;
}

/**
 * Whether `data` is already in Normalization Form C, by the quick check that UAX #15
 * gives. A code point that is NFC_QC=No or that follows a mark of a higher combining
 * class rules the string out; one that is NFC_QC=Maybe is not enough to decide and
 * so is treated as ruling it out too, this being a check that only ever answers for
 * certain. A string it passes needs no work, so can be copied straight to the output.
 *
 * https://www.unicode.org/reports/tr15/#Detecting_Normalization_Forms
 */
static bool
normalize__is_nfc(const utf32_t *data, size_t len) {
  uint8_t last = 0;

  for (size_t i = 0; i < len; i++) {
    utf32_t c = data[i];

    // Everything below the first mark is a starter that is left alone, which is the
    // whole of an ASCII string and most of a typical one.
    if (c < NORMALIZE_FIRST_MARK) {
      last = 0;

      continue;
    }

    // NFC_QC=No never stands in a normalized string, and NFC_QC=Maybe might compose
    // with the code point before it; neither can be passed by the quick check. This
    // is tested ahead of the combining class so that a code point that rules the
    // string out, such as a decomposed mark, does so without the second lookup.
    if (normalize__quick_check(c) != NORMALIZE_QC_YES) return false;

    uint8_t combining_class = normalize_combining_class(c);

    // A mark out of canonical order might reorder and then compose, so the string
    // cannot be taken for normalized without doing the work.
    if (last > combining_class && combining_class != 0) return false;

    last = combining_class;
  }

  return true;
}

size_t
normalize_nfc(const utf32_t *data, size_t len, utf32_t *result) {
  // An input that is already in the form is by far the common case for the callers
  // that normalize to check or to canonicalize, such as the IDNA path, and needs no
  // more than to be copied out.
  if (normalize__is_nfc(data, len)) {
    memcpy(result, data, len * sizeof(utf32_t));

    return len;
  }

  size_t out = 0;

  for (size_t i = 0; i < len; i++) {
    out += normalize_decompose(data[i], &result[out]);
  }

  return normalize_recompose(result, out);
}
