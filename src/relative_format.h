// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A relative time formatter's state (maul-locale/relative.h, record
// mloc-0043), for the relative time formatter and the joining of a
// relative day with a time.

#ifndef MLOC_RELATIVE_FORMAT_H
#define MLOC_RELATIVE_FORMAT_H

#include "data_view.h"
#include "number_core.h"
#include "number_format.h"

#include "maul-locale/relative.h"

enum
{
    relativeStamp = 0x52,
    // The most relative units a locale's chain files.
    relativeChainMost = 8,
    // The room for a text read and its titlecase.
    relativeTextMost = 256,
    // The room for a direction's six patterns.
    relativePatternsMost = 6 * relativeTextMost,
};

typedef struct RelativeFormat
{
    Formatter formatter;
    // The chain's relative units, the locale's first.
    DataSpan units[relativeChainMost];
    // The titlecase unit; empty when the formatter does not capitalize.
    DataSpan titlecase;
    uint8_t unitCount;
    uint8_t width;
    uint8_t numeric;
    uint8_t capitalize;
    // 1 for Turkish and Azerbaijani, whose i titlecases to İ.
    uint8_t turkic;
} RelativeFormat;

// The formatter of an opaque value, or false when the library did not
// make it.
bool mlocReadRelativeFormat(const mlocRelativeFormatter* opaque, RelativeFormat* f);

// Whether an amount and a unit are ones the formatter takes.
bool mlocValidRelativeAmount(const mlocDecimal* amount, uint32_t unit);

// Writes a valid amount of a unit as one part of a kind; as
// mlocFormatRelative's result but for the output's checks.
mlocResult mlocWriteRelative(const RelativeFormat* f, const mlocDecimal* amount, uint32_t unit,
                             uint32_t kind, NumberWriter* w);

#endif
