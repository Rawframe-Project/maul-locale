// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out part's units (record mloc-0034), shared by the library
// and the generator. All integers are little-endian.
//
// A group of rules (spell-out, ordinal suffixes, numbering systems), a
// unit per locale key whose own file has the group (the lookup along the
// chain inherits a group whole, as ICU does):
//
//   header      spelloutHeaderSize bytes: the counts below, the default
//               rule set, the text size
//   sets        spelloutSetSize each
//   rules       spelloutRuleSize each; a set's normal rules (bases
//               ascending), then its special rules in the order they
//               came (the last negative, infinity and NaN rule, every
//               fraction rule)
//   pieces      spelloutPieceSize each: a rule's text, substitutions and
//               plural block in the order they are written
//   plurals     spelloutPluralSize each: a text per plural category
//   patterns    spelloutPatternSize each: a decimal pattern compiled
//   text        UTF-8, which every offset indexes
//
// The numbering systems, a unit of no locale: u16 count, u16 text size,
// then per algorithmic system, sorted by its CLDR id, a record of
// spelloutSystemSize bytes (u16 offsets of the id, the locale whose rules
// it names, "" for the locale asked for, and the rule set's name; u8
// their lengths; u8 the group), then the text.
//
// The titlecase table, a unit of no locale: u16 count, u16 text size,
// then per lowercase letter (General_Category Ll) of the rules' text whose
// full titlecase differs, sorted by code point, a record of
// spelloutTitlecaseSize bytes (u32 code point, u16 offset and u8 length
// of its titlecase, u8 zero), then the text.

#ifndef MLOC_SPELLOUT_TABLES_H
#define MLOC_SPELLOUT_TABLES_H

#include <stdint.h>

enum
{
    // Header: u16 sets, rules, pieces, plurals, patterns, default set,
    // text size, zero.
    spelloutHeaderSize = 16,
    // Set: u16 name offset, u8 name length, u8 flags, u16 first rule,
    // u16 normal rules, u16 special rules.
    spelloutSetSize = 10,
    // Rule: u64 base, u16 first piece, u16 radix (a normal rule's) or
    // decimal point (a fraction rule's), u8 kind, u8 flags, u8 pieces,
    // u8 exponent (of ten with spelloutPowerOfTen, else of the radix).
    spelloutRuleSize = 16,
    // Piece: u8 type, u8 detail, u16 index or length, u16 offset or rule.
    spelloutPieceSize = 6,
    // Plural: per category u16 offset, u16 length (spelloutNone for
    // none); u32 the categories in the order the block writes them, a
    // nibble each from the lowest, 0xF past the last (parsing searches
    // the forms in that order).
    spelloutPluralSize = 28,
    // Pattern: u8 minimum integer digits, minimum and maximum fraction
    // digits, primary and secondary grouping (0 for none), flags.
    spelloutPatternSize = 6,
    spelloutSystemHeaderSize = 4,
    spelloutSystemSize = 10,
    spelloutTitlecaseHeaderSize = 4,
    spelloutTitlecaseSize = 8,
    spelloutMaxText = 0xFFFF,
    spelloutNone = 0xFFFF,
};

// Set flags.
enum
{
    spelloutSetPublic = 1,
    spelloutSetFraction = 2,
    spelloutSetParseable = 4,
};

// Rule kinds, ICU's: a base value, "-x", "x.x", "0.x", "x.0", "Inf",
// "NaN".
typedef enum SpelloutRuleKind
{
    spelloutNormal = 0,
    spelloutNegative = 1,
    spelloutImproper = 2,
    spelloutProper = 3,
    spelloutDefault = 4,
    spelloutInfinity = 5,
    spelloutNaN = 6,
    spelloutKindCount = 7,
} SpelloutRuleKind;

// Rule flags.
enum
{
    // The rule has a remainder substitution and a base that is not a
    // multiple of its divisor: a multiple of the divisor takes the rule
    // before (ICU's rollback).
    spelloutRollback = 1,
    // The divisor is ten to the rule's exponent; else the radix to it.
    spelloutPowerOfTen = 2,
};

// Piece types.
typedef enum SpelloutPieceType
{
    // detail 0, the index its length, the offset its text.
    spelloutText = 0,
    // detail the substitution's kind and flags, the index its rule set
    // or pattern, the offset the rule a ">>>" remainder takes
    // (spelloutNone for none).
    spelloutSubstitution = 1,
    // detail 1 for ordinal rules, the index the plural record.
    spelloutPlural = 2,
    spelloutPieceTypeCount = 3,
} SpelloutPieceType;

// Substitution kinds (the low three bits of a substitution's detail).
typedef enum SpelloutSubKind
{
    spelloutMultiplier = 0,
    spelloutModulus = 1,
    spelloutSame = 2,
    spelloutIntegral = 3,
    spelloutFraction = 4,
    spelloutAbsolute = 5,
    spelloutNumerator = 6,
    spelloutSubKindCount = 7,
} SpelloutSubKind;

// Substitution flags (the high bits of its detail).
enum
{
    spelloutSubKindMask = 7,
    // ">>>": a remainder by the rule before, digits without spaces.
    spelloutSubTriple = 8,
    // A numerator's leading zeros are written.
    spelloutSubZeros = 16,
    // A fraction digit by digit.
    spelloutSubByDigits = 32,
    // The index is a pattern's, not a rule set's.
    spelloutSubPattern = 64,
};

// Pattern flags.
enum
{
    // The decimal separator is written with no fraction digits ("0.").
    spelloutPatternPoint = 1,
};

// The groups.
typedef enum SpelloutGroup
{
    spelloutGroupSpellout = 0,
    spelloutGroupOrdinal = 1,
    spelloutGroupNumbering = 2,
    spelloutGroupCount = 3,
} SpelloutGroup;

#endif
