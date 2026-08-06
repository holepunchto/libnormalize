#include <normalize/tables.h>
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

    utf32_t composite = normalize_compose(data[starter], c);

    if (composite != 0 && (last == 0 || last < combining_class)) {
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

size_t
normalize_nfc(const utf32_t *data, size_t len, utf32_t *result) {
  size_t out = 0;

  for (size_t i = 0; i < len; i++) {
    out += normalize_decompose(data[i], &result[out]);
  }

  return normalize_recompose(result, out);
}
