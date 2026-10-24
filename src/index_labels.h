// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An index's candidates, as ICU4C's AlphabeticIndex::init gathers them:
// the initial labels (the collator's U+FDD0 contractions, with A to Z when
// one ends in a letter A to Z, else the locale's labels from the data) and
// the script boundaries (the root's U+FDD1 contractions whose second code
// point is a letter or unassigned). They are held where they are found,
// sorted and filtered by index_build.c.

#ifndef MLOC_SRC_INDEX_LABELS_H
#define MLOC_SRC_INDEX_LABELS_H

#include "collator.h"
#include "index_value.h"

#include "maul-locale/data.h"
#include "maul-locale/locale.h"

#include <stdint.h>

enum
{
    indexMaxLabels = 512,
    indexMaxScripts = 256,
    // A label's UTF-8 at most, as the data's units hold it.
    indexMaxLabelBytes = 255,
    // A contraction's UTF-8 at most: its first code point and up to
    // eight after it.
    indexWordsBytes = 36,
};

// A label or a boundary where it is found: UTF-8 (a data label, A to Z),
// or a code point and the code points after it as 32-bit little-endian
// words (a contraction's entry in a context list).
typedef struct IndexText
{
    const uint8_t* bytes;
    // Bytes, or words.
    uint32_t length;
    // The first code point of a contraction; 0 for UTF-8.
    uint32_t first;
    // Set by index_build.c: its key, and whether more than one primary is
    // above the variable top.
    IndexKey key;
    bool multi;
} IndexText;

typedef struct IndexCandidates
{
    IndexText labels[indexMaxLabels];
    uint32_t labelCount;
    // The boundaries, U+FDD1 and its sample, in the root's order.
    IndexText scripts[indexMaxScripts];
    uint32_t scriptCount;
} IndexCandidates;

// Gathers the candidates: mloc_success, or mloc_errorData past the
// limits above.
mlocResult mlocIndexGather(const Collator* collator, const mlocDataSet* data,
                           const mlocLocale* locale, IndexCandidates* out);

// A text's UTF-8: its own bytes, or a contraction's encoded into scratch
// (indexWordsBytes); its length into lengthOut.
const uint8_t* mlocIndexTextBytes(const IndexText* text, uint8_t* scratch, uint32_t* lengthOut);

#endif
