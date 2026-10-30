// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The likely subtags table: its validator, for the kind list, and the
// lookups language matching makes on it.

#ifndef MLOC_LOCALE_LIKELY_H
#define MLOC_LOCALE_LIKELY_H

#include "data_view.h"

#include "maul-locale/locale.h"

#include <stdint.h>

typedef struct Likely
{
    const uint8_t* generalKeys;
    const uint8_t* generalValues;
    const uint8_t* languageValues;
    const uint8_t* languageKeys;
    uint32_t generalCount;
    uint32_t languageCount;
} Likely;

bool mlocValidLikely(const DataView* view, DataSpan payload);

// The first source's likely subtags table, or false.
bool mlocOpenLikely(const mlocDataSet* data, Likely* likelyOut);

// The value of the entry for a packed language, script and region (0
// for none): the full tag, packed.
bool mlocFindLikely(const Likely* likely, uint32_t language, uint32_t script, uint32_t region,
                    uint64_t* valueOut);

// The table's answer for a packed language ("und" packed, not 0),
// script and region (0, Zzzz or ZZ for none) as maximizing finds it:
// the full tag, packed.
bool mlocLookupLikely(const Likely* likely, uint32_t language, uint32_t script, uint32_t region,
                      uint64_t* valueOut);

// Whether two locales share a byte: an output that does with an input
// is refused, so a failed call leaves it as it was.
bool mlocLocalesOverlap(const mlocLocale* one, const mlocLocale* two);

// Whether an entry has the language and, unless script is 0, the
// script.
bool mlocHasLikely(const Likely* likely, uint32_t language, uint32_t script);

// The general entries with a language alone and a region, as a range of
// indexes, and the value at an index.
void mlocLikelyRegionRange(const Likely* likely, uint32_t language, uint32_t* startOut,
                           uint32_t* endOut);
uint64_t mlocLikelyValueAt(const Likely* likely, uint32_t index);

#endif
