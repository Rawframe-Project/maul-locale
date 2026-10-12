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

#endif
