// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An opened data source as the parts read it, and the lookups they use:
// a locale's index by its id, a unit by its kind and locale, and the
// first source of a set that has a unit.

#ifndef MLOC_DATA_VIEW_H
#define MLOC_DATA_VIEW_H

#include "data_format.h"

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
    // dataViewCookie, which only an open writes; it also fills the struct,
    // so that a view has no padding bytes and two views of the same bytes
    // compare equal.
    uint32_t cookie;
} DataView;

// An opened view's cookie: its top bit set, so that no zeroed or small
// value is taken for one.
static const uint32_t dataViewCookie = 0xC00C1E4D;

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
// type reads the public struct's words: false for one without the cookie
// (left uninitialized or cleared) or whose sizes disagree, so that
// nothing is read through it. The bytes pointer cannot be checked: one
// changed after the open is outside the contract.
MLOC_NODISCARD static inline bool mlocReadView(const mlocData* data, DataView* viewOut)
{
    memcpy(viewOut, data, sizeof(*viewOut));
    const DataView* v = viewOut;
    return v->cookie == dataViewCookie && v->bytes != nullptr && v->poolOffset <= v->size &&
           v->poolSize == v->size - v->poolOffset &&
           dataHeaderSize + (uint64_t)v->unitCount * dataUnitSize +
                   (uint64_t)v->localeCount * dataLocaleSize <=
               v->poolOffset;
}

// The view of an mlocData already read with mlocReadView or checked with
// mlocValidDataSet, or just opened.
static inline DataView mlocViewOf(const mlocData* data)
{
    DataView view;
    memcpy(&view, data, sizeof(view));
    return view;
}

// Whether a set is one a call may read: not null, and each of its
// sources an opened view (mlocReadView).
MLOC_NODISCARD bool mlocValidDataSet(const mlocDataSet* set);

// The index of a locale id, or false when the source has none.
MLOC_NODISCARD bool mlocFindDataLocale(const DataView* view, const char* id, size_t length,
                                       uint32_t* indexOut);

// A unit by its kind and locale index (dataNoLocale for a unit of no
// locale), or false.
MLOC_NODISCARD bool mlocFindDataUnit(const DataView* view, uint32_t kind, uint32_t locale,
                                     DataSpan* spanOut);

// mlocFindDataUnit; on a miss, whether the source has no unit of the kind
// at all.
MLOC_NODISCARD bool mlocFindDataUnitOrKind(const DataView* view, uint32_t kind, uint32_t locale,
                                           DataSpan* spanOut, bool* noKindOut);

// Whether a source of a set has a unit of the kind.
MLOC_NODISCARD bool mlocDataSetHasKind(const mlocDataSet* set, uint32_t kind);

// Whether a source holds a unit of a kind.
MLOC_NODISCARD bool mlocDataHasKind(const DataView* view, uint32_t kind);

// The first source of a set with a unit of the kind for the locale id
// (null for a unit of no locale), or false.
MLOC_NODISCARD bool mlocFindDataSetUnit(const mlocDataSet* set, uint32_t kind, const char* id,
                                        size_t length, DataSpan* spanOut);

// A unit by its place in the unit table: its kind and payload, or false
// past the table. The open checked every entry, so a view an open made
// reads only its own payloads.
MLOC_NODISCARD bool mlocDataUnitAt(const DataView* view, uint32_t index, uint32_t* kindOut,
                                   DataSpan* spanOut);

// The first source of a set with a unit of the kind and no locale: the
// source's place in the set, the unit's in its table, and its payload.
MLOC_NODISCARD bool mlocFindDataSetUnitAt(const mlocDataSet* set, uint32_t kind, size_t* sourceOut,
                                          uint32_t* indexOut, DataSpan* spanOut);

#endif
