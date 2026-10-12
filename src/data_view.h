// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An opened data source as the parts read it, and the lookups they use:
// a locale's index by its id, a unit by its kind and locale, and the
// first source of a set that has a unit.

#ifndef MLOC_DATA_VIEW_H
#define MLOC_DATA_VIEW_H

#include "maul-locale/data.h"

#include <stdint.h>
#include <string.h>

typedef struct DataView
{
    const uint8_t* bytes;
    uint32_t size;
    uint32_t unitCount;
    uint32_t localeCount;
    uint32_t poolOffset;
    uint32_t poolSize;
} DataView;

static_assert(sizeof(DataView) <= sizeof(mlocData), "a view fits its public struct");
static_assert(alignof(DataView) <= alignof(mlocData), "and its alignment");

// A unit's payload.
typedef struct DataSpan
{
    const uint8_t* bytes;
    uint32_t size;
} DataSpan;

// Checks one kind's payload in a view whose tables already hold; returns
// whether it is valid.
typedef bool (*DataValidator)(const DataView* view, DataSpan payload);

typedef struct DataKind
{
    uint32_t kind;
    DataValidator validate;
} DataKind;

// The kinds the library reads, sorted by kind (data_kinds.c).
extern const DataKind* const mlocDataKinds;
extern const uint32_t mlocDataKindCount;

// mlocOpenData with the kinds given (the tests give their own).
mlocResult mlocOpenDataWithKinds(DataView* viewOut, const void* bytes, size_t size,
                                 const DataKind* kinds, uint32_t kindCount);

// The view an mlocData holds, copied out so that no pointer of another
// type reads the public struct's words.
static inline DataView mlocViewOf(const mlocData* data)
{
    DataView view;
    memcpy(&view, data, sizeof(view));
    return view;
}

// The index of a locale id, or false when the source has none.
bool mlocFindDataLocale(const DataView* view, const char* id, size_t length, uint32_t* indexOut);

// A unit by its kind and locale index (dataNoLocale for a unit of no
// locale), or false.
bool mlocFindDataUnit(const DataView* view, uint32_t kind, uint32_t locale, DataSpan* spanOut);

// The first source of a set with a unit of the kind for the locale id
// (null for a unit of no locale), or false.
bool mlocFindDataSetUnit(const mlocDataSet* set, uint32_t kind, const char* id, size_t length,
                         DataSpan* spanOut);

#endif
