// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list part's units (src/list_tables.h): their validators, and a
// locale's patterns of a style.

#ifndef MLOC_LIST_DATA_H
#define MLOC_LIST_DATA_H

#include "data_view.h"
#include "list_tables.h"

#include "maul-locale/locale.h"

#include <stdint.h>

// Whether a pattern sets unit is a table whose records index its strings
// and whose strings each hold "{0}" and "{1}" once, and no other brace.
bool mlocValidListPatterns(const DataView* view, DataSpan payload);

// Whether a locale's lists unit has its size.
bool mlocValidLists(const DataView* view, DataSpan payload);

// A language's contextual change of the "two" and "end" patterns, as
// ICU's ListFormatter: Spanish's "y" and "o", Hebrew's vav.
typedef enum ListRule
{
    listRuleNone = 0,
    listRuleSpanish = 1,
    listRuleHebrew = 2,
} ListRule;

// A locale's patterns of a style (two, start, middle, end) and its rule.
typedef struct ListPatterns
{
    const char* text[listPatternCount];
    uint16_t length[listPatternCount];
    ListRule rule;
} ListPatterns;

// Finds a locale's patterns of a style along its main chain. False when
// the data has no lists, its units disagree, or the chain cannot be
// walked.
bool mlocFindListPatterns(const mlocDataSet* data, const mlocLocale* locale, ListStyle style,
                          ListPatterns* out);

#endif
