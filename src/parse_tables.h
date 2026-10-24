// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The sets number parsing reads, shared by the library and the generator.
// All integers are little-endian.
//
// The parse sets, a unit of no locale: a header (u16 class count, u16
// range count, u16 string count, u16 text size, u16 fold count, u16
// upper count, u16 special count, u16 0), per class in the order of
// ParseClass its first range, range count, first string and string count
// (u16 each), the ranges (first and last code point, u32 each; a class's
// sorted and disjoint), the folds, the uppers, the specials, the string
// offsets into the text (u16 each, count + 1, from 0 to the text size),
// then the text: each string valid UTF-8 of more than one code point (a
// set's "{Rs}"). The folds are the simple case folding ICU's parser
// compares with (u_foldCase with the Turkic mappings for I and dotted I),
// the uppers the full uppercase of one code point that ICU's currency
// names compare with (u_strToUpper's default rules), each as runs,
// sorted and disjoint: a first code point (u32), a count (u16), a stride
// of 1 or 2 (u8), a 0, and the difference each code point of the run
// maps by (i32). The specials are the uppercase mappings of more than
// one code point ("ß" to "SS"), sorted by code point: the code point and
// two or three code points, 0 after the last (u32 each). The classes
// are root's parseLenients of the general and number scopes, as ICU
// reads them, and ICU's fixed sets: the ignorables
// ([[:Zs:]\t[:Bidi_Control:][:Variation_Selector:]]), the bidi controls
// alone, the other grouping separators, infinity, the approximately
// signs, and the decimal digits, each range of them a run whose digit is
// its offset from the range's first modulo 10.
//
// A data layout: tools/check_data_format.py watches this header.

#ifndef MLOC_PARSE_TABLES_H
#define MLOC_PARSE_TABLES_H

typedef enum ParseClass
{
    parseIgnorables,
    parseStrictIgnorables,
    parsePeriod,
    parseStrictPeriod,
    parseComma,
    parseStrictComma,
    parseOtherGrouping,
    parseMinus,
    parsePlus,
    parsePercent,
    parsePerMille,
    parseInfinity,
    parseApproximately,
    parseDigits,
    parseDollar,
    parsePound,
    parseYen,
    parseWon,
    parseRupee,
    parseClassCount,
} ParseClass;

enum
{
    parseHeaderSize = 16,
    parseClassSize = 8,
    parseRangeSize = 8,
    parseFoldSize = 12,
    parseSpecialSize = 16,
};

#endif
