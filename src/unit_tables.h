// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's unit tables (record mloc-0023), shared by the library
// and the generator: units of no locale holding what units.xml gives.
// All integers are little-endian.
//
// The unit table reads and converts unit identifiers. Its header
// (unitsHeaderSize bytes): the counts of simple units, prefixes, aliases,
// base units, Beaufort steps, dimensions and quantities (u16 each), 2
// bytes 0, then the sizes of the text and of the numbers (u32 each).
// Then, in order:
//
// - The simple units, sorted by identifier, unitSimpleSize bytes each:
//   the identifier's text offset (u32) and length (u8), the base unit's
//   index (u8; unitNoBase for a unit that does not convert), flags
//   (u8), the unit's place in UTS #35's normalization order (u8), the
//   offset of its factor in the numbers (u32), its systems (u16, the
//   unitSystem bits) and 2 bytes 0.
// - The prefixes, sorted by text, unitPrefixSize bytes each: the text
//   offset (u32) and length (u8), the power of ten (i8) and the power of
//   1024 (u8), and a byte 0. Exactly one of the powers is not 0.
// - The aliases, sorted by the deprecated identifier, unitAliasSize
//   bytes each: the deprecated and the replacing identifiers' text
//   offsets (u32) and lengths (u8 each), and 2 bytes 0.
// - The base units, unitBaseSize bytes each: the identifier's text
//   offset (u32) and length (u8), 3 bytes 0, then the exponent of each
//   dimension in its numerator and in its denominator as written, not
//   cancelled (u8 each, unitMaxDimensions of each; those past the count
//   0): "cubic-meter-per-meter" is 3 and 1 meters.
// - The dimensions, unitDimensionSize bytes each: the simple base unit's
//   identifier, a text offset (u32) and length (u8), its place in the
//   normalization order (u8), and 2 bytes 0.
// - The quantities, in units.xml's order, unitQuantitySize bytes each:
//   the name's and the base unit identifier's text offsets (u32 each)
//   and lengths (u8 each), 2 bytes 0, and the base unit's numerator and
//   denominator exponents as for base units.
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
//
// The unit preferences (UTS #35 Part 6, Unit Preferences): a header
// (unitPrefsHeaderSize bytes: the counts of sets, runs and entries,
// u16 each, 2 bytes 0, and the text size, u32), then:
//
// - The sets, sorted by category then usage, unitSetSize bytes each: the
//   category's and the usage's text offsets (u32 each) and lengths (u8
//   each), the first run's index and the run count (u16 each), and 2
//   bytes 0. Each set's runs follow the one before's, the first set's
//   from 0, the last's to the run count.
// - The runs, unitRunSize bytes each: a region packed (u16, ascending
//   within a set, 001 among them) and the first entry's index (u16,
//   ascending across all runs from 0); a run's entries end at the next
//   run's first or the entry count.
// - The entries, unitEntrySize bytes each, in CLDR's order: the unit's
//   identifier, a text offset (u32) and length (u8), the threshold's
//   decimal places (u8), the skeleton's rounding increment (u16; 0 for
//   none), and the threshold scaled by its places (u32): 0.9 is 9 with
//   1 place. A run's last entry has the threshold 0.
// - The text.

#ifndef MLOC_UNIT_TABLES_H
#define MLOC_UNIT_TABLES_H

#include <stdint.h>

enum
{
    unitsHeaderSize = 24,
    unitSimpleSize = 16,
    unitPrefixSize = 8,
    unitAliasSize = 12,
    unitBaseSize = 40,
    unitDimensionSize = 8,
    unitQuantitySize = 44,
    unitMaxDimensions = 16,
    unitMaxNumberBytes = 64,
    unitNoBase = 0xFF,
    unitPrefsHeaderSize = 12,
    unitSetSize = 16,
    unitRunSize = 4,
    unitEntrySize = 12,
    // The most decimal places a threshold has.
    unitMaxPlaces = 9,
};

// The flags of a simple unit.
enum
{
    // Its factor carries an offset (a temperature scale).
    unitOffset = 1,
    // It converts by the Beaufort table, not a factor.
    unitBeaufort = 2,
};

// The systems a unit belongs to (units.xml's systems attribute).
enum
{
    unitSystemSi = 1,
    unitSystemSiAcceptable = 2,
    unitSystemMetric = 4,
    unitSystemMetricAdjacent = 8,
    unitSystemUs = 16,
    unitSystemUk = 32,
    unitSystemJapan = 64,
    unitSystemAstronomical = 128,
    unitSystemPersonAge = 256,
    unitSystemPrefixable = 512,
    unitSystemAll = 1023,
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
    simpleSystems = 12,
};

// The fields of a base unit and a quantity: their exponents.
enum
{
    baseNumerator = 8,
    baseDenominator = baseNumerator + unitMaxDimensions,
    quantityNumerator = 12,
    quantityDenominator = quantityNumerator + unitMaxDimensions,
};

#endif
