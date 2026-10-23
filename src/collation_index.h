// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale's index labels: the set ICU4C's AlphabeticIndex takes from a
// locale whose collator has no U+FDD0 contractions, resolved by the
// generator (the index exemplars, or the standard ones made into labels),
// a unit per locale key in the collation data file, found along the main
// chain as ICU4C's bundles fall back.
//
// The unit: a 32-bit count, then each label as a byte, its length, and
// that many bytes of UTF-8, the last ending at the unit's end. The
// labels are well-formed, nonempty, in code point order and distinct. A
// label may end in one "*", which keeps a label of several code points
// that sorts as its parts; the index's filter reads it.

#ifndef MLOC_COLLATION_INDEX_H
#define MLOC_COLLATION_INDEX_H

#include "data_view.h"

#include "maul-locale/locale.h"

#include <stdint.h>

typedef struct IndexLabels
{
    const uint8_t* bytes;
    uint32_t size;
    uint32_t count;
} IndexLabels;

bool mlocValidIndexLabels(const DataView* view, DataSpan payload);

// The labels of a locale along its main chain. False when no step has
// a unit (data without the collation file), or when the chain needs the
// parents table and the data has none.
bool mlocFindIndexLabels(const mlocDataSet* data, const mlocLocale* locale, IndexLabels* out);

// The label at a place in the unit (4 for the first) and the place
// after it; false past the last.
bool mlocNextIndexLabel(const IndexLabels* labels, uint32_t* at, const uint8_t** textOut,
                        uint32_t* lengthOut);

#endif
