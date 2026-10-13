// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validators of the number units (record mloc-0021,
// number_tables.h), for the kind list, and the reads the formatters make
// of them. Each validator reads its own unit alone; the indexes one unit
// holds into another are bounded when a formatter is made.

#ifndef MLOC_NUMBER_DATA_H
#define MLOC_NUMBER_DATA_H

#include "data_view.h"

#include <stdint.h>

bool mlocValidNumberSystems(const DataView* view, DataSpan payload);
bool mlocValidNumberSymbols(const DataView* view, DataSpan payload);
bool mlocValidNumberPatterns(const DataView* view, DataSpan payload);
bool mlocValidNumbers(const DataView* view, DataSpan payload);
bool mlocValidNumberCompactPatterns(const DataView* view, DataSpan payload);
bool mlocValidNumberCompact(const DataView* view, DataSpan payload);
bool mlocValidCurrencyFractions(const DataView* view, DataSpan payload);
bool mlocValidCurrencyPatterns(const DataView* view, DataSpan payload);
bool mlocValidCurrencySymbols(const DataView* view, DataSpan payload);
bool mlocValidCurrencyNames(const DataView* view, DataSpan payload);

// A table unit's string count, record count and the start of its text
// (the symbol and pattern units share this shape).
typedef struct NumberTable
{
    uint32_t stringCount;
    uint32_t recordCount;
    const uint8_t* offsets;
    const uint8_t* records;
    const uint8_t* text;
} NumberTable;

// The parts of a valid table unit.
NumberTable mlocReadNumberTable(DataSpan unit, uint32_t recordSize);

// Whether a table unit's sizes hold and its strings are well-formed
// UTF-8 without NUL, their bytes below 0x20 at most maxPlaceholder (0 for
// none). Its records are the caller's to check.
bool mlocValidNumberTable(DataSpan payload, uint32_t recordSize, uint8_t maxPlaceholder);

// A compact table of a valid compact unit, or null for an index past its
// tables.
const uint8_t* mlocFindCompactTable(DataSpan unit, uint32_t index);

// A currency's digits and roundings.
typedef struct CurrencyFraction
{
    uint8_t digits;
    uint8_t rounding;
    uint8_t cashDigits;
    uint8_t cashRounding;
} CurrencyFraction;

// A currency's fraction in a valid fractions unit, the default entry's
// for a code it lacks.
CurrencyFraction mlocFindCurrencyFraction(DataSpan unit, uint16_t code);

// The parts of a sparse unit (currency_tables.h).
typedef struct SparseTable
{
    uint32_t stringCount;
    uint32_t entryCount;
    uint32_t textSize;
    uint32_t rowSize;
    const uint8_t* offsets;
    const uint8_t* codes;
    const uint8_t* rowOffsets;
    const uint8_t* rows;
    const uint8_t* text;
} SparseTable;

SparseTable mlocReadSparseTable(DataSpan unit);

// What a kind's sparse units hold: the fields, those that are strings, the
// least and the largest code, a byte of letter flags per string after the text, and
// whether a string may be empty.
typedef struct SparseShape
{
    uint32_t fieldCount;
    uint32_t stringFields;
    uint32_t minCode;
    uint32_t maxCode;
    bool flags;
    bool empty;
} SparseShape;

// Whether a sparse unit's sizes hold, its strings are valid text, its
// codes ascend and its rows are valid, as its shape says.
bool mlocValidSparse(DataSpan payload, const SparseShape* shape);

// The row of a code in a valid sparse unit, or null.
const uint8_t* mlocFindSparseRow(const SparseTable* table, uint16_t code);

// A row's value of a field; false when the row does not give it.
bool mlocSparseField(const uint8_t* row, uint32_t field, uint16_t* valueOut);

#endif
