// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's unit table (record mloc-0023), shared by the library
// and the generator: a unit of no locale holding what units.xml gives to
// read and convert unit identifiers. All integers are little-endian.
//
// The header (unitsHeaderSize bytes): the counts of simple units,
// prefixes, aliases, base units and Beaufort steps (u16 each), the
// count of dimensions (u16), then the sizes of the text and of the
// numbers (u32 each). Then, in order:
//
// - The simple units, sorted by identifier, unitSimpleSize bytes each:
//   the identifier's text offset (u32) and length (u8), the base unit's
//   index (u8; unitNoBase for a unit that does not convert), flags
//   (u8), the unit's place in UTS #35's normalization order (u8), and
//   the offset of its factor in the numbers (u32).
// - The prefixes, sorted by text, unitPrefixSize bytes each: the text
//   offset (u32) and length (u8), the power of ten (i8) and the power of
//   1024 (u8), and a byte 0. Exactly one of the powers is not 0.
// - The aliases, sorted by the deprecated identifier, unitAliasSize
//   bytes each: the deprecated and the replacing identifiers' text
//   offsets (u32) and lengths (u8 each), and 2 bytes 0.
// - The base units, unitBaseSize bytes each: the identifier's text
//   offset (u32) and length (u8), 3 bytes 0, and its exponent of each
//   dimension (i8 each, unitMaxDimensions; those past the count 0).
// - The Beaufort steps: the least speed of each force from 0 up, then
//   one past the last force's range, in tenths of a meter per second
//   (u16 each, ascending; UTS #35 gives no table, so the generator holds
//   the one ICU uses, CLDR-17421).
// - The text, then the numbers.
//
// A factor in the numbers: the numerator and the denominator of the
// unit's size in its base unit, then with unitOffset the numerator and
// the denominator of the offset added after scaling; each an unsigned
// integer as a length (u8, at most unitMaxNumberBytes) and that many
// bytes, least significant first, the top one not 0; a denominator is
// not 0.

#ifndef MLOC_UNIT_TABLES_H
#define MLOC_UNIT_TABLES_H

#include <stdint.h>

enum
{
    unitsHeaderSize = 20,
    unitSimpleSize = 12,
    unitPrefixSize = 8,
    unitAliasSize = 12,
    unitBaseSize = 24,
    unitMaxDimensions = 16,
    unitMaxNumberBytes = 64,
    unitNoBase = 0xFF,
};

// The flags of a simple unit.
enum
{
    // Its factor carries an offset (a temperature scale).
    unitOffset = 1,
    // It converts by the Beaufort table, not a factor.
    unitBeaufort = 2,
};

// The fields of a simple unit.
enum
{
    simpleText = 0,
    simpleLength = 4,
    simpleBase = 5,
    simpleFlags = 6,
    simpleOrder = 7,
    simpleFactor = 8,
};

#endif
