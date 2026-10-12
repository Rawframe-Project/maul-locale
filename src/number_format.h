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
    // Which formatter made it.
    uint8_t stamp;
} Formatter;

// Whether a number formatter's options are in range and agree.
bool mlocValidFormatOptions(const mlocNumberOptions* options);

// Copies valid options into a formatter, with the stamp of its kind.
void mlocSetFormatOptions(Formatter* formatter, const mlocNumberOptions* options, uint8_t stamp);

// Reads a formatter from an opaque value of a size; false when the
// library did not make it as a formatter of the stamp's kind.
bool mlocReadFormatter(const void* opaque, size_t size, uint8_t stamp, Formatter* formatterOut);

// Formats a decimal, after checking the decimal and the output.
mlocResult mlocFormatWith(const Formatter* formatter, const mlocDecimal* decimal,
                          mlocOutput* output);

// Formats a range of two decimals, after checking them and the output.
mlocResult mlocFormatRangeWith(const Formatter* formatter, const mlocDecimal* start,
                               const mlocDecimal* end, mlocOutput* output);

#endif
