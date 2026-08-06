// Generates the character tables needed to normalize from the Unicode Character
// Database, as documented in <https://www.unicode.org/reports/tr44>.
//
// Usage:
//
//   node scripts/generate-tables.mjs <version> <ucd> <header> <source>
//
// where <version> is the Unicode version the data was taken from, <ucd> is a
// directory holding the data files, and <header> and <source> are the files to
// write.

import {
  constants,
  blocks,
  codePoints,
  entryBlocks,
  hex,
  open,
  pool,
  range,
  rangeBlocks,
  ranges,
  table
} from 'cmake-ucd'
import fs from 'fs'

const { CODE_POINTS } = constants

const [version, data, header, source] = process.argv.slice(2)

if (!version || !data || !header || !source) {
  console.error('Usage: node scripts/generate-tables.mjs <version> <ucd> <header> <source>')
  process.exit(1)
}

const { lines } = open(data)

/**
 * UnicodeData.txt
 *
 * https://www.unicode.org/reports/tr44/#UnicodeData.txt
 */

const combiningClasses = new Uint8Array(CODE_POINTS)
const decompositions = new Map()

{
  let first = null

  for (const fields of lines('UnicodeData.txt')) {
    const c = parseInt(fields[0], 16)
    const name = fields[1]
    const combiningClass = parseInt(fields[3], 10)
    const decomposition = fields[5]

    // Large blocks of code points sharing the same properties are given as a pair
    // of `First` and `Last` lines rather than one line per code point.
    if (name.endsWith(', First>')) {
      first = c
      continue
    }

    let start = c

    if (name.endsWith(', Last>')) {
      start = first
      first = null
    }

    for (let i = start; i <= c; i++) combiningClasses[i] = combiningClass

    // Only canonical decompositions take part in normalization; compatibility
    // decompositions are tagged with a bracketed formatting tag.
    if (decomposition !== '' && !decomposition.startsWith('<')) {
      decompositions.set(c, codePoints(decomposition))
    }
  }
}

/**
 * CompositionExclusions.txt
 *
 * https://www.unicode.org/reports/tr44/#CompositionExclusions.txt
 */

const exclusions = new Set()

for (const [codes] of lines('CompositionExclusions.txt')) {
  const [start, end] = range(codes)

  for (let c = start; c <= end; c++) exclusions.add(c)
}

/**
 * Table construction
 */

// The number of low bits of a code point that a block index leaves to the search
// it narrows down.
const BLOCK_SHIFT = 9
const BLOCKS = blocks(BLOCK_SHIFT)

// A range of the combining class table is packed as `(start << 8) | value`, a
// combining class fitting in the byte that leaves.
function packRange([start, value]) {
  return hex(start * 0x100 + value)
}

const combiningClassRanges = ranges(combiningClasses)

const decompositionData = pool()
const decompositionEntries = []

// Decompositions are recursive, so are expanded up front to leave the runtime with
// a single lookup per code point.
function decompose(c) {
  const decomposition = decompositions.get(c)

  if (decomposition === undefined) return [c]

  return decomposition.flatMap(decompose)
}

// The longest full canonical decomposition of any single code point, which is what
// a caller has to leave room for. Taken from the data rather than assumed, so that
// a version adding a longer one cannot go unnoticed.
let maxDecomposition = 1

for (const c of [...decompositions.keys()].sort((a, b) => a - b)) {
  const decomposition = decompose(c)

  if (decomposition.length >= 0x100) {
    throw new Error(`Decomposition of ${c.toString(16)} is too long`)
  }

  maxDecomposition = Math.max(maxDecomposition, decomposition.length)

  const offset = decompositionData.add(decomposition)

  decompositionEntries.push([c, decomposition.length * 0x1000000 + offset])
}

// The pairs of code points that compose into a primary composite. Singleton
// decompositions, decompositions that begin with a non-starter, and the code points
// listed in CompositionExclusions.txt are all excluded.
const compositions = []

for (const [c, decomposition] of decompositions) {
  if (decomposition.length !== 2) continue
  if (combiningClasses[decomposition[0]] !== 0) continue
  if (exclusions.has(c)) continue

  compositions.push([decomposition[0], decomposition[1], c])
}

compositions.sort((a, b) => a[0] - b[0] || a[1] - b[1])

if (decompositionData.data.length >= 0x1000000) {
  throw new Error('Decomposition data is too large')
}

const combiningClassBlocks = rangeBlocks(
  combiningClassRanges.map(([start]) => start),
  BLOCK_SHIFT
)

const decompositionBlocks = entryBlocks(
  decompositionEntries.map(([c]) => c),
  BLOCK_SHIFT
)

for (const [name, entries] of [
  ['combining class', combiningClassRanges],
  ['decomposition', decompositionEntries]
]) {
  if (entries.length > 0xffff) {
    throw new Error(`The ${name} table is too large to index by block`)
  }
}

// The first code point that is a combining mark, and so the first with a combining
// class other than zero. It is also the first that takes part in a composition as
// the second of its two code points, which is what lets a lookup of either be
// skipped for everything below it.
const firstMark = combiningClassRanges.find(([, value]) => value !== 0)[0]

// The first code point with a canonical decomposition.
const firstDecomposable = decompositionEntries[0][0]

/**
 * Output
 */

const banner = `// This file was generated from the Unicode Character Database ${version} by
// scripts/generate-tables.mjs. Do not edit.
`

fs.writeFileSync(
  header,
  `${banner}
#ifndef NORMALIZE_TABLES_H
#define NORMALIZE_TABLES_H

#include <stdint.h>
#include <utf.h>

#define NORMALIZE_UNICODE_VERSION "${version}"

/**
 * The longest full canonical decomposition of a single code point, which is the
 * room that \`normalize_decompose()\` has to be given.
 */
#define NORMALIZE_MAX_DECOMPOSITION ${maxDecomposition}

/**
 * The first code point that is a combining mark, and so the first with a combining
 * class other than zero. It is also the first that takes part in a composition as
 * the second of its two code points.
 */
#define NORMALIZE_FIRST_MARK ${hex(firstMark)}

/**
 * The first code point with a canonical decomposition.
 */
#define NORMALIZE_FIRST_DECOMPOSABLE ${hex(firstDecomposable)}

/**
 * A range of code points sharing the same property value, packed as
 * \`(start << 8) | value\`. Ranges are sorted and gap-free, each one extending up
 * to the start of the range that follows it.
 */
typedef uint32_t normalize_range_t;

/**
 * A block index narrows a search down to the entries that may hold a given code
 * point. The high \`21 - NORMALIZE_BLOCK_SHIFT\` bits of the code point index the
 * block, and the entries of interest are those from the block's own index up to and
 * including that of the block after it.
 */
#define NORMALIZE_BLOCK_SHIFT ${BLOCK_SHIFT}

#define NORMALIZE_BLOCKS ${BLOCKS}

/**
 * The Canonical_Combining_Class property of a code point.
 */
extern const normalize_range_t normalize__combining_class_ranges[${combiningClassRanges.length}];

extern const uint16_t normalize__combining_class_blocks[NORMALIZE_BLOCKS];

/**
 * The full canonical decomposition of a code point, given as a length and an offset
 * into \`normalize__decomposition_data\` packed as \`(length << 24) | offset\`.
 * Entries are sorted by code point.
 */
typedef struct {
  utf32_t code_point;
  uint32_t decomposition;
} normalize__decomposition_t;

extern const normalize__decomposition_t normalize__decompositions[${decompositionEntries.length}];

extern const uint16_t normalize__decomposition_blocks[NORMALIZE_BLOCKS];

extern const utf32_t normalize__decomposition_data[${decompositionData.data.length}];

/**
 * A pair of code points and the primary composite they compose into. Entries are
 * sorted by first and then second code point.
 */
typedef struct {
  utf32_t first;
  utf32_t second;
  utf32_t composite;
} normalize__composition_t;

extern const normalize__composition_t normalize__compositions[${compositions.length}];

#define NORMALIZE_COMPOSITIONS ${compositions.length}

#endif // NORMALIZE_TABLES_H
`
)

fs.writeFileSync(
  source,
  `${banner}
#include <normalize/tables.h>
#include <stdint.h>
#include <utf.h>

const normalize_range_t normalize__combining_class_ranges[${combiningClassRanges.length}] = {
${table(combiningClassRanges.map(packRange), 8)}};

const uint16_t normalize__combining_class_blocks[NORMALIZE_BLOCKS] = {
${table(combiningClassBlocks.map(String), 12)}};

const normalize__decomposition_t normalize__decompositions[${decompositionEntries.length}] = {
${table(
  decompositionEntries.map(([c, d]) => `{${hex(c)}, ${hex(d)}}`),
  4
)}};

const uint16_t normalize__decomposition_blocks[NORMALIZE_BLOCKS] = {
${table(decompositionBlocks.map(String), 12)}};

const utf32_t normalize__decomposition_data[${decompositionData.data.length}] = {
${table(decompositionData.data.map(hex), 8)}};

const normalize__composition_t normalize__compositions[${compositions.length}] = {
${table(
  compositions.map(([a, b, c]) => `{${hex(a)}, ${hex(b)}, ${hex(c)}}`),
  4
)}};
`
)
