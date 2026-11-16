#ifndef NORMALIZE_H
#define NORMALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <normalize/tables.h>
#include <stddef.h>
#include <stdint.h>
#include <utf.h>

#if defined(_WIN32)
#if defined(NORMALIZE_BUILDING)
#define NORMALIZE_EXPORT __declspec(dllexport)
#elif defined(NORMALIZE_SHARED)
#define NORMALIZE_EXPORT __declspec(dllimport)
#else
#define NORMALIZE_EXPORT
#endif
#elif defined(__GNUC__)
#define NORMALIZE_EXPORT __attribute__((visibility("default")))
#else
#define NORMALIZE_EXPORT
#endif

/**
 * The Canonical_Combining_Class of a code point, which is what canonical ordering
 * sorts a decomposition by. A starter has a class of zero.
 *
 * https://www.unicode.org/reports/tr44/#Canonical_Combining_Class
 */
NORMALIZE_EXPORT uint8_t
normalize_combining_class(utf32_t c);

/**
 * The most code points that normalizing `len` of them can produce, for sizing the
 * result of `normalize_nfd()` and `normalize_nfc()`. Decomposing is the only step
 * that lengthens, so the bound is the longest decomposition of a single code point
 * for every one of them.
 *
 * Returns `(size_t) -1` if `len` is large enough that the bound would overflow.
 */
NORMALIZE_EXPORT size_t
normalize_max_length(size_t len);

/**
 * Writes the full canonical decomposition of `c` to `result` and returns how many
 * code points that took. A code point that does not decompose is written as it
 * stands, so this never writes nothing.
 *
 * `result` must have room for `NORMALIZE_MAX_DECOMPOSITION` code points.
 *
 * https://www.unicode.org/reports/tr15/#Canonical_Decomposition
 */
NORMALIZE_EXPORT size_t
normalize_decompose(utf32_t c, utf32_t *result);

/**
 * The primary composite of `first` and `second`, or 0 if the two do not compose.
 *
 * https://www.unicode.org/reports/tr15/#Primary_Composite
 */
NORMALIZE_EXPORT utf32_t
normalize_compose(utf32_t first, utf32_t second);

/**
 * Puts the canonical decomposition held in `data` into Normalization Form C in
 * place, returning its new length. This is the second half of normalizing, the
 * first being to decompose every code point with `normalize_decompose()`.
 *
 * A caller that decomposes as it goes, such as one mapping its input at the same
 * time, is served by this rather than by `normalize_nfc()`.
 *
 * https://www.unicode.org/reports/tr15/#Description_Norm
 */
NORMALIZE_EXPORT size_t
normalize_recompose(utf32_t *data, size_t len);

/**
 * Writes the `len` code points of `data` to `result` in Normalization Form D, and
 * returns how many code points that took.
 *
 * `result` must have room for `normalize_max_length(len)` code points.
 *
 * https://www.unicode.org/reports/tr15/#Norm_Forms
 */
NORMALIZE_EXPORT size_t
normalize_nfd(const utf32_t *data, size_t len, utf32_t *result);

/**
 * Writes the `len` code points of `data` to `result` in Normalization Form C, and
 * returns how many code points that took.
 *
 * `result` must have room for `normalize_max_length(len)` code points; composing
 * only ever shortens what decomposing produced, so the same bound covers both.
 *
 * https://www.unicode.org/reports/tr15/#Norm_Forms
 */
NORMALIZE_EXPORT size_t
normalize_nfc(const utf32_t *data, size_t len, utf32_t *result);

#ifdef __cplusplus
}
#endif

#endif // NORMALIZE_H
