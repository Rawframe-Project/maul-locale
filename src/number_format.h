// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The formatting the number formatters share (record mloc-0021): a core
// with its options; a decimal is rounded by the precision or compact
// notation, then written, alone or as a range's end.

#ifndef MLOC_NUMBER_FORMAT_H
#define MLOC_NUMBER_FORMAT_H

#include "number_core.h"

#include "maul-locale/number.h"
#include "maul-locale/plural.h"

#include <stddef.h>

typedef struct Formatter
{
    NumberCore core;
    // Compact notation's table and patterns; null otherwise.
    const uint8_t* compactTable;
    const uint8_t* compactPatterns;
    mlocPrecision precision;
    uint8_t notation;
    uint8_t style;
    uint8_t grouping;
    uint8_t signDisplay;
    uint8_t minInteger;
    // The locale's cardinal rule set, for compact patterns.
    uint8_t pluralRule;
    // 1 for a percent or per mille style that writes the value as it is
    // (the unit percent, as ICU), 0 to scale it.
    uint8_t unscaled;
    // Which formatter made it.
    uint8_t stamp;
} Formatter;

enum
{
    // Marks a number formatter the library made.
    numberFormatterStamp = 0x4E,
};

// Whether a number formatter's options are in range and agree.
bool mlocValidFormatOptions(const mlocNumberOptions* options);

// Copies valid options into a formatter, with the stamp of its kind.
void mlocSetFormatOptions(Formatter* formatter, const mlocNumberOptions* options, uint8_t stamp);

// Reads a formatter from an opaque value of a size; false when the
// library did not make it as a formatter of the stamp's kind. The stamp
// refuses a zeroed or unmade value, so the fields checked are those that
// bound a read or a write or pick a branch (lengths, counts, options);
// the core's texts are the data's whenever the stamp is.
bool mlocReadFormatter(const void* opaque, size_t size, uint8_t stamp, Formatter* formatterOut);

// A pattern around a formatted number, by the plural category of the
// number as rounded (a currency's long name, "{0} US dollars"; the ISO
// append, the same for every category; a unit's name): each category's
// pattern, where currencyTextNumber stands for the number and
// currencyTextOther for its text (currency_tables.h), and that text, a
// part of textKind; the code where a category's text is null. The
// pattern's own text is a part of patternKind between the spaces at its
// ends; a pattern without the number writes none (Arabic's dual unit
// names, as ICU).
typedef struct Outer
{
    const uint8_t* patternText;
    uint16_t patternStart[6];
    uint16_t patternLength[6];
    const uint8_t* text[6];
    uint16_t textLength[6];
    uint32_t textKind;
    uint32_t patternKind;
    char code[3];
    // The plural rules whose ranges give a range's category; any for a
    // pattern the same in every category.
    mlocPluralRules rules;
} Outer;

// The plural category of a valid decimal as the formatter writes it, the
// one an outer pattern is chosen by: mloc_success, or mloc_errorCapacity
// when rounding passes the decimal's limits (the category then other).
mlocResult mlocFormattedCategory(const Formatter* formatter, const mlocDecimal* decimal,
                                 uint32_t* categoryOut);

// Writes a valid decimal as mlocFormatWith does, into a writer of the
// caller's: mloc_success, or mloc_errorCapacity when rounding passes the
// decimal's limits.
mlocResult mlocWriteFormatted(const Formatter* formatter, const Outer* outer,
                              const mlocDecimal* decimal, NumberWriter* writer);

// Writes the locale's range separator, spaced where it has no space of
// its own when spaced is set; the parts that follow are the range end's.
void mlocWriteRangeSeparator(const NumberCore* core, bool spaced, NumberWriter* writer);

// Formats a decimal, within an outer pattern when one is given (the
// formatter then has its plural rule set), after checking the decimal
// and the output.
mlocResult mlocFormatWith(const Formatter* formatter, const Outer* outer,
                          const mlocDecimal* decimal, mlocOutput* output);

// Chooses the outer pattern of a number written in a plural category:
// the pattern, or null with the failure in resultOut.
typedef const Outer* (*OuterChooser)(void* context, uint32_t category, mlocResult* resultOut);

// Formats a valid decimal into a checked output within the outer pattern
// choose gives for the category the number is written in, rounding once.
mlocResult mlocFormatChosen(const Formatter* formatter, OuterChooser choose, void* context,
                            const mlocDecimal* decimal, mlocOutput* output);

// Formats a range of two decimals, within an outer pattern when one is
// given, once around both ends, in the category the plural ranges give
// theirs. Checks the decimals and the output first.
mlocResult mlocFormatRangeWith(const Formatter* formatter, const Outer* outer,
                               const mlocDecimal* start, const mlocDecimal* end,
                               mlocOutput* output);

// The decimal a formatter's text shows for a valid finite decimal:
// rounded, scaled for a percent or per mille, without its sign; false in
// scientific or compact notation, whose text shows another number.
bool mlocShownDecimal(const Formatter* formatter, const mlocDecimal* decimal, mlocDecimal* out);

#endif
