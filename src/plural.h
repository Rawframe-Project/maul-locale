// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validators of the plural units (record mloc-0020), for the kind
// list, and the rule sets by index, for the number part.

#ifndef MLOC_PLURAL_H
#define MLOC_PLURAL_H

#include "data_view.h"

#include "maul-locale/plural.h"

bool mlocValidPlurals(const DataView* view, DataSpan payload);
bool mlocValidPluralRanges(const DataView* view, DataSpan payload);

// The index of a locale's rule set of a type, as mlocInitPluralRules
// finds it, for a part that keeps only the index.
mlocResult mlocFindPluralRule(const mlocDataSet* data, const mlocLocale* locale,
                              mlocPluralType type, uint8_t* ruleOut);

// The category a rule set gives operands; the index from
// mlocFindPluralRule.
mlocPluralCategory mlocSelectPluralRule(uint8_t rule, const mlocPluralOperands* operands);

// A range's category by valid cardinal rules' table, other where it has no
// entry, as ICU's StandardPluralRanges resolves the plural of a unit or
// currency name around a range.
mlocPluralCategory mlocResolvePluralRange(const mlocPluralRules* rules, mlocPluralCategory start,
                                          mlocPluralCategory end);

#endif
