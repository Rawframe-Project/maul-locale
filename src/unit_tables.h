// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's unit tables, shared by the library and the generator:
// units of no locale holding what units.xml gives. All integers are
// little-endian.
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
//
// The unit names ship as a data file of their own (units.mloc). Its unit
// of no locale lists the identifiers the names units know: a header (u16
// count, u16 0, u32 text size), the identifiers' text offsets (u32 each,
// count + 1, from 0 to the text size, ascending), their kinds (u8 each,
// the unitId values), then the text: identifiers strictly ascending by
// their bytes, each of ASCII lowercase letters, digits, '-' and ':'. An
// identifier is a core unit identifier ("meter", "kilometer-per-hour"), a
// compound key ("per", "times", "power2", "10p3", "1024p3") or a power of
// a gender ("power2:feminine"); its index is its code in the names units.
//
// The names of a width (long, short, narrow): a unit per locale key in
// the sparse shape (currency_tables.h, codes from 0) with unitNameFields
// fields, all strings: the display name, the pattern for each plural
// category zero to other, and the per-unit pattern ("{0} per meter"); a
// prefix's, "per"'s and "times"'s pattern is its other field. The values
// are those of ICU's data (CLDR's production data read along the chain,
// width and unit aliases followed), each filed at a key where it differs
// from what the key's chain gives, a plural category the chain lacks
// taking the other category's; a power of a gender the chain lacks takes
// the plain power's field. An empty plural category is other's: it marks
// a category ICU's data has where the text is the same, which a compound
// name needs (ICU takes a unit's placeholder from the first category it
// has); an empty display name or per-unit pattern is none. Patterns keep
// "{0}" and "{1}".
//
// The long width's extras, in the same shape with unitExtraFields
// fields: the unit's grammatical gender, and each pattern of a unit or
// prefix lowercased by the locale's case rules (ICU lowercases a core
// unit under a prefix or power in long names), an empty string where
// lowercasing changes nothing. Every locale files its categories and
// one, which compound names read for a denominator and for the units of
// a product before the last.
//
// The long width's patterns of a case, in the same shape with
// unitCaseFields fields, the patterns for each plural category zero to
// other: a row per identifier and case past the nominative, coded by the
// identifier's index plus the count of identifiers times the case's
// index less one. A field is ICU's pattern of the case (getMeasureData:
// the case's own value along the chain, else the plain one's; a power's
// as InflectedPluralSink loads it) where the plain lookup would give
// another; an empty field or none takes the plain lookup. Only units and
// powers with case patterns in some locale have rows. Those patterns
// lowercased, likewise, for the units: a field is the lowercased form of
// the case pattern the library takes, an empty field or none where
// lowercasing changes nothing.
//
// The derivations (grammaticalFeatures.xml), a unit of no locale: a
// header (u16 count, u16 0), then per language, sorted by it,
// unitDerivationSize bytes: the language (ASCII, NUL-padded); the plural
// category each structure gives its two components (per, times, power,
// prefix; value0 then value1, u8 each): a category's index zero to
// other, unitDerivedCompound for the compound's own, unitDerivedNone
// where the language gives no rule; the case each structure gives its
// components, in the same order: a case's index (unitCaseNominative and
// the others), unitDerivedCompound or unitDerivedNone; then the gender
// each structure gives the compound (u8 each): unitGenderFirst or
// unitGenderSecond for a component's, a gender's index (unitGenderAnimate
// and the others) or unitDerivedNone. A language CLDR lists takes its
// rules, a feature it gives no rule for none; any other language takes
// root's, as ICU's DerivedComponents.
//
// A locale's duration and coordinate patterns, a unit per locale key
// filed where they differ from its chain's, in the table shape of
// number_tables.h with one record: the string indexes (u16) of the
// numeric duration patterns for hours and minutes, minutes and seconds,
// and all three ("h:mm", "m:ss", "h:mm:ss"; "h", "m" and "s" stand for
// the fields, doubled for two digits, and text in apostrophes is
// literal), then the coordinate patterns ("{0}N") of each width, long,
// short and narrow, for east, north, south and west. The values are
// CLDR's resolution.
//
// A data layout: tools/check_data_format.py watches this header.

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
    unitIdsHeaderSize = 8,
    // The longest identifier the names know.
    unitMaxNameId = 64,
    unitDerivationsHeaderSize = 4,
    unitDerivationSize = 28,
    unitDerivationLanguage = 8,
    unitDerivationPlural = 8,
    unitDerivationCase = 16,
    unitDerivationGender = 24,
    unitDerivedCompound = 0xFE,
    unitDerivedNone = 0xFF,
    unitGenderFirst = 0xF0,
    unitGenderSecond = 0xF1,
};

// The grammatical cases CLDR names (grammaticalFeatures.xml): the
// nominative, then the others in alphabetical order, as
// mlocGrammaticalCase numbers them.
enum
{
    unitCaseNominative = 0,
    unitCaseCount = 28,
};

// The grammatical genders CLDR names, from 1 in alphabetical order
// (animate, common, feminine, inanimate, masculine, neuter, personal).
enum
{
    unitGenderAnimate = 1,
    unitGenderCount = 8,
};

// The structures of a derivation, in record order (two values each).
enum
{
    unitDerivePer = 0,
    unitDeriveTimes = 1,
    unitDerivePower = 2,
    unitDerivePrefix = 3,
    unitDeriveStructures = 4,
};

// The kinds of a names identifier.
enum
{
    unitIdUnit = 0,
    unitIdPrefix = 1,
    unitIdPower = 2,
    // "per" and "times".
    unitIdCompound = 3,
};

// The fields of a names unit and of the extras.
enum
{
    unitNameDisplay = 0,
    unitNameZero = 1,
    unitNameOther = 6,
    unitNamePer = 7,
    unitNameFields = 8,
    unitExtraGender = 0,
    unitExtraLowerZero = 1,
    unitExtraFields = 7,
    unitCaseFields = 6,
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

enum
{
    unitPatternHm = 0,
    unitPatternMs = 1,
    unitPatternHms = 2,
    // The first coordinate pattern: width (0 long, 1 short, 2 narrow) * 4
    // + direction.
    unitPatternCoordinates = 3,
    unitDirectionEast = 0,
    unitDirectionNorth = 1,
    unitDirectionSouth = 2,
    unitDirectionWest = 3,
    unitPatternCount = 15,
};

#endif
