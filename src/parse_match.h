// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The matchers of number parsing (record mloc-0032), ICU4C 79's
// NumberParserImpl matchers: each reads the text at a segment's offset
// into a parse, moving the offset past what it read, or leaves both. The
// parser (number_parse.c) builds their list from a formatter and runs it.

#ifndef MLOC_PARSE_MATCH_H
#define MLOC_PARSE_MATCH_H

#include "number_format.h"

#include "maul-locale/decimal.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // The matchers of a list and of an affix's series.
    parseMaxMatchers = 24,
    parseMaxSeries = 32,
    // The affix patterns of a parser: a prefix and a suffix per sign type.
    parseMaxAffixes = 4,
    // The affix matchers: the pairs and the unpaired affixes.
    parseMaxAffixMatchers = 6,
    // The bytes of an affix pattern a series is built from.
    parseMaxAffixText = 64,
    // A code point no set holds: a byte that does not begin valid UTF-8.
    parseNoCodePoint = 0x110000,
};

// The flags of a parse (ICU's ParsedNumber flags).
enum
{
    parsedNegative = 1,
    parsedPercent = 2,
    parsedPerMille = 4,
    parsedHasExponent = 8,
    parsedHasDecimal = 16,
    parsedNaN = 32,
    parsedInfinity = 64,
    parsedFail = 128,
};

// The modes' flags (ICU's parse flags).
enum
{
    modeIntegerOnly = 1,
    modeGroupingDisabled = 2,
    modeStrictGrouping = 4,
    modeStrictSeparators = 8,
    modeUnpairedAffixes = 16,
    modeFullAffixes = 32,
    modeExactAffix = 64,
    modeStrictIgnorables = 128,
    modePlusAllowed = 256,
};

typedef enum MatcherKind
{
    matchIgnorables,
    matchMinus,
    matchPlus,
    matchApproximately,
    matchPercent,
    matchPerMille,
    matchNaN,
    matchInfinity,
    matchDecimal,
    matchScientific,
    matchCodePoint,
    matchAffix,
    // Validators, which read nothing and check the parse after the run.
    validateNumber,
    validateAffix,
    validateScale,
} MatcherKind;

typedef struct Matcher
{
    uint8_t kind;
    // A sign matcher in an affix accepts a sign after the number.
    bool trailing;
    // An affix matcher's index, a code point matcher's code point, a
    // scale validator's power of ten.
    uint32_t arg;
} Matcher;

// An affix pattern as a series of token matchers; its text, the pattern's
// bytes, identifies it.
typedef struct Series
{
    Matcher matchers[parseMaxSeries];
    uint32_t count;
    uint8_t text[parseMaxAffixText];
    uint16_t length;
    // ICU's pattern length in UTF-16 units, which orders affix matchers.
    uint16_t units;
} Series;

// No affix, or the empty affix a parse records once an affix matcher
// accepts it without one (ICU's bogus and empty strings).
enum
{
    affixNone = -1,
    affixEmpty = -2,
};

// A prefix and a suffix series (or affixNone) and the flags a parse
// takes when they are the ones it matched.
typedef struct AffixMatcher
{
    int8_t prefix;
    int8_t suffix;
    uint32_t flags;
} AffixMatcher;

// The digits read: the significant ones as typed, leading zeros dropped
// and trailing zeros counted, so that the decimal's capacity bounds only
// the digits that matter.
typedef struct Quantity
{
    uint8_t digits[mlocDecimalDigits];
    uint32_t count;
    uint32_t zeros;
    bool present;
} Quantity;

typedef struct Parsed
{
    Quantity quantity;
    // The digits read after the decimal separator, and the exponent read.
    int64_t afterPoint;
    int64_t exponent;
    uint32_t flags;
    // Where the last matcher that ends a number stopped (ICU's charEnd).
    size_t charEnd;
    // The affixes matched: series indexes, affixNone or affixEmpty.
    int8_t prefix;
    int8_t suffix;
    // More digits, or a larger exponent, than a decimal holds.
    bool overflow;
} Parsed;

typedef struct Segment
{
    const uint8_t* text;
    size_t length;
    size_t at;
} Segment;

// A separator's set besides its own text: a class, all the separators,
// or the separator's first code point alone (ICU's unisets keys).
enum
{
    setAll = 0x100,
    setOwn = 0x101,
};

typedef struct Parser
{
    const Formatter* f;
    const uint8_t* sets;
    uint32_t flags;
    Matcher list[parseMaxMatchers];
    uint32_t count;
    Series series[parseMaxAffixes];
    uint32_t seriesCount;
    AffixMatcher affixes[parseMaxAffixMatchers];
    uint32_t affixCount;
    // The decimal matcher's separators, their first code points, and the
    // sets they are read from.
    const uint8_t* decimalText;
    uint8_t decimalLength;
    const uint8_t* groupText;
    uint8_t groupLength;
    uint32_t decimalCodePoint;
    uint32_t groupCodePoint;
    uint32_t decimalSet;
    uint32_t groupSet;
    // Digits are also matched by the numbering system's strings: its zero
    // is not a decimal digit of value 0.
    bool digitStrings;
} Parser;

// The code point at an offset and its length; parseNoCodePoint and 1 for
// a byte that does not begin valid UTF-8.
uint32_t mlocParseCodePoint(const uint8_t* text, size_t length, size_t at, size_t* sizeOut);

// Whether a class of the parse sets holds a code point.
bool mlocParseHas(const uint8_t* sets, uint32_t set, uint32_t cp);

// The value of a decimal digit's code point, -1 for another.
int mlocParseDigit(const uint8_t* sets, uint32_t cp);

// The first set of two that holds a one code point text, else setOwn
// (ICU's unisets::chooseFrom).
uint32_t mlocParseChooseSet(const uint8_t* sets, const uint8_t* text, size_t length, uint32_t first,
                            uint32_t second);

// Whether the affix a matcher names is the one a parse matched (ICU's
// AffixMatcher::matched): both none, or equal patterns.
bool mlocParseMatched(const Parser* parser, int8_t affix, int8_t seen);

// Runs a matcher at a segment with text left.
void mlocParseMatch(const Parser* parser, const Matcher* matcher, Segment* segment, Parsed* parsed);

#endif
