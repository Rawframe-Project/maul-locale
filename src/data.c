// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Opening and validating data, and the lookups over it. Validation reads
// every table once; the lookups trust what it checked.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "invariant.h"
#include "kinds.h"

#include "generated/data_version.h"

#include <string.h>

extern const uint8_t mlocBuiltinData[];
extern const uint32_t mlocBuiltinDataSize;

static mlocVersion UnpackVersion(uint32_t packed)
{
    return (mlocVersion){(uint16_t)(packed >> 16), (uint16_t)(packed >> 8 & 0xFF),
                         (uint16_t)(packed & 0xFF)};
}

void mlocGetDataVersion(mlocDataVersion* versionOut)
{
    *versionOut = (mlocDataVersion){
        .cldr = {MLOC_CLDR_VERSION_MAJOR, MLOC_CLDR_VERSION_MINOR, MLOC_CLDR_VERSION_PATCH},
        .unicode = {MLOC_DATA_UNICODE_MAJOR, MLOC_DATA_UNICODE_MINOR, MLOC_DATA_UNICODE_PATCH},
        .format = MLOC_DATA_FORMAT_VERSION,
        .tables = MLOC_DATA_TABLES_HASH,
    };
    memcpy(versionOut->tzdb, MLOC_DATA_TZDB, sizeof(MLOC_DATA_TZDB) - 1);
}

static bool IsHeader(const uint8_t* bytes, size_t size)
{
    return size >= dataHeaderSize && memcmp(bytes + headerMagic, MLOC_DATA_MAGIC, 4) == 0 &&
           mlocLoad16(bytes + headerFormat) == dataFormatVersion &&
           mlocLoad16(bytes + headerHeaderSize) == dataHeaderSize;
}

static mlocDataVersion HeaderVersion(const uint8_t* bytes)
{
    mlocDataVersion version = {
        .cldr = UnpackVersion(mlocLoad32(bytes + headerCldr)),
        .unicode = UnpackVersion(mlocLoad32(bytes + headerUnicode)),
        .format = mlocLoad32(bytes + headerDataFormat),
        .tables = mlocLoad64(bytes + headerTables),
    };
    memcpy(version.tzdb, bytes + headerTzdb, sizeof(version.tzdb));
    return version;
}

mlocResult mlocReadDataVersion(const void* bytes, size_t size, mlocDataVersion* versionOut)
{
    if (bytes == nullptr || versionOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    if (!IsHeader(bytes, size))
    {
        return mloc_errorData;
    }
    *versionOut = HeaderVersion(bytes);
    return mloc_success;
}

// Whether two versions are equal but perhaps in the tzdb release.
static bool SameButRelease(const mlocDataVersion* a, const mlocDataVersion* b)
{
    return memcmp(&a->cldr, &b->cldr, sizeof(a->cldr)) == 0 &&
           memcmp(&a->unicode, &b->unicode, sizeof(a->unicode)) == 0 && a->format == b->format &&
           a->tables == b->tables;
}

// Whether a file holds zones units alone: a zones file, which may be of
// another tzdb release than the library's.
static bool OnlyZones(const DataView* view)
{
    for (uint32_t i = 0; i < view->unitCount; i++)
    {
        if (mlocLoad32(view->bytes + dataHeaderSize + (size_t)i * dataUnitSize) != kindZones)
        {
            return false;
        }
    }
    return view->unitCount > 0;
}

// The id of locale i, which validation has bounded.
static const uint8_t* LocaleId(const DataView* view, uint32_t i, uint32_t* lengthOut)
{
    MLOC_ASSERT(i < view->localeCount);
    const uint8_t* entry = view->bytes + dataHeaderSize + (size_t)view->unitCount * dataUnitSize +
                           (size_t)i * dataLocaleSize;
    uint32_t offset = mlocLoad32(entry);
    *lengthOut = mlocLoad32(entry + 4);
    MLOC_ASSERT(*lengthOut > 0 && *lengthOut <= dataMaxLocaleId && offset <= view->poolSize &&
                *lengthOut <= view->poolSize - offset);
    return view->bytes + view->poolOffset + offset;
}

// Orders ids by their bytes, a prefix first. Ids are short: a loop
// beats a call to memcmp.
static int CompareIds(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    size_t common = aLength < bLength ? aLength : bLength;
    for (size_t k = 0; k < common; k++)
    {
        if (a[k] != b[k])
        {
            return a[k] < b[k] ? -1 : 1;
        }
    }
    return aLength < bLength ? -1 : aLength > bLength;
}

static bool IsIdByte(uint8_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool ValidLocales(const DataView* view)
{
    const uint8_t* previous = nullptr;
    uint32_t previousLength = 0;
    for (uint32_t i = 0; i < view->localeCount; i++)
    {
        const uint8_t* entry = view->bytes + dataHeaderSize +
                               (size_t)view->unitCount * dataUnitSize + (size_t)i * dataLocaleSize;
        uint32_t offset = mlocLoad32(entry);
        uint32_t length = mlocLoad32(entry + 4);
        if (length == 0 || length > dataMaxLocaleId || offset > view->poolSize ||
            length > view->poolSize - offset)
        {
            return false;
        }
        const uint8_t* id = view->bytes + view->poolOffset + offset;
        for (uint32_t k = 0; k < length; k++)
        {
            if (!IsIdByte(id[k]))
            {
                return false;
            }
        }
        if (previous != nullptr && CompareIds(previous, previousLength, id, length) >= 0)
        {
            return false;
        }
        previous = id;
        previousLength = length;
    }
    return true;
}

static const DataKind* FindKind(const DataKind* kinds, uint32_t count, uint32_t kind)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (kinds[i].kind == kind)
        {
            return &kinds[i];
        }
    }
    return nullptr;
}

static bool ValidUnits(const DataView* view, uint32_t payloadStart, const DataKind* kinds,
                       uint32_t kindCount)
{
    uint64_t previous = 0;
    for (uint32_t i = 0; i < view->unitCount; i++)
    {
        const uint8_t* entry = view->bytes + dataHeaderSize + (size_t)i * dataUnitSize;
        uint32_t kind = mlocLoad32(entry);
        uint32_t locale = mlocLoad32(entry + 4);
        uint32_t offset = mlocLoad32(entry + 8);
        uint32_t size = mlocLoad32(entry + 12);
        // Units of no locale sort after a kind's locales, as the index
        // dataNoLocale is the largest.
        uint64_t key = (uint64_t)kind << 32 | locale;
        if ((i > 0 && key <= previous) || (locale != dataNoLocale && locale >= view->localeCount) ||
            offset % dataAlignment != 0 || offset < payloadStart || offset > view->poolOffset ||
            size > view->poolOffset - offset)
        {
            return false;
        }
        previous = key;
        const DataKind* known = FindKind(kinds, kindCount, kind);
        if (known == nullptr || !known->validate(view, (DataSpan){view->bytes + offset, size}))
        {
            return false;
        }
    }
    return true;
}

// The tables' bounds and order; the header's own fields are checked.
static bool ValidTables(const DataView* view, const DataKind* kinds, uint32_t kindCount)
{
    uint64_t tablesEnd = dataHeaderSize + (uint64_t)view->unitCount * dataUnitSize +
                         (uint64_t)view->localeCount * dataLocaleSize;
    uint64_t payloadStart = (tablesEnd + dataAlignment - 1) / dataAlignment * dataAlignment;
    if (payloadStart > view->poolOffset || view->poolOffset > view->size ||
        view->poolSize != view->size - view->poolOffset)
    {
        return false;
    }
    return ValidLocales(view) && ValidUnits(view, (uint32_t)payloadStart, kinds, kindCount);
}

mlocResult mlocOpenDataWithKinds(DataView* viewOut, const void* bytes, size_t size,
                                 const DataKind* kinds, uint32_t kindCount)
{
    if (viewOut == nullptr || bytes == nullptr || (uintptr_t)bytes % dataAlignment != 0)
    {
        return mloc_errorInvalid;
    }
    const uint8_t* b = bytes;
    if (!IsHeader(b, size))
    {
        return mloc_errorData;
    }
    mlocDataVersion own;
    mlocGetDataVersion(&own);
    mlocDataVersion theirs = HeaderVersion(b);
    bool sameRelease = memcmp(own.tzdb, theirs.tzdb, sizeof(own.tzdb)) == 0;
    if (!SameButRelease(&own, &theirs))
    {
        return mloc_errorDataVersion;
    }
    if (mlocLoad32(b + headerFileSize) != size || mlocLoad32(b + headerFlags) != 0 ||
        mlocLoad32(b + headerReserved) != 0)
    {
        return mloc_errorData;
    }
    DataView view = {
        .bytes = b,
        .size = (uint32_t)size,
        .unitCount = mlocLoad32(b + headerUnitCount),
        .localeCount = mlocLoad32(b + headerLocaleCount),
        .poolOffset = mlocLoad32(b + headerPoolOffset),
        .poolSize = mlocLoad32(b + headerPoolSize),
    };
    if (!ValidTables(&view, kinds, kindCount))
    {
        return mloc_errorData;
    }
    if (!sameRelease && !OnlyZones(&view))
    {
        return mloc_errorDataVersion;
    }
    *viewOut = view;
    return mloc_success;
}

mlocResult mlocOpenData(mlocData* dataOut, const void* bytes, size_t size)
{
    if (dataOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    DataView view;
    mlocResult result = mlocOpenDataWithKinds(&view, bytes, size, mlocDataKinds, mlocDataKindCount);
    if (result == mloc_success)
    {
        *dataOut = (mlocData){0};
        memcpy(dataOut, &view, sizeof(view));
    }
    return result;
}

void mlocOpenBuiltinData(mlocData* dataOut)
{
    // The generator and the tests validate the compiled-in bytes, so
    // opening them reads only the header.
    const uint8_t* b = mlocBuiltinData;
    DataView view = {
        .bytes = b,
        .size = mlocBuiltinDataSize,
        .unitCount = mlocLoad32(b + headerUnitCount),
        .localeCount = mlocLoad32(b + headerLocaleCount),
        .poolOffset = mlocLoad32(b + headerPoolOffset),
        .poolSize = mlocLoad32(b + headerPoolSize),
    };
    MLOC_ASSERT(IsHeader(b, mlocBuiltinDataSize) &&
                mlocLoad32(b + headerFileSize) == mlocBuiltinDataSize &&
                view.poolOffset <= view.size && view.poolSize == view.size - view.poolOffset);
    *dataOut = (mlocData){0};
    memcpy(dataOut, &view, sizeof(view));
}

size_t mlocGetDataLocaleCount(const mlocData* data)
{
    return mlocViewOf(data).localeCount;
}

mlocResult mlocGetDataLocale(const mlocData* data, size_t index, const char** idOut,
                             size_t* lengthOut)
{
    if (data == nullptr || idOut == nullptr || lengthOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    DataView view = mlocViewOf(data);
    if (index >= view.localeCount)
    {
        return mloc_errorInvalid;
    }
    uint32_t length = 0;
    *idOut = (const char*)LocaleId(&view, (uint32_t)index, &length);
    *lengthOut = length;
    return mloc_success;
}

bool mlocFindDataLocale(const DataView* view, const char* id, size_t length, uint32_t* indexOut)
{
    uint32_t low = 0;
    uint32_t high = view->localeCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t middleLength = 0;
        const uint8_t* middleId = LocaleId(view, middle, &middleLength);
        int order = CompareIds(middleId, middleLength, (const uint8_t*)id, length);
        if (order == 0)
        {
            *indexOut = middle;
            return true;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

// The search of a unit table, taking the view's two fields by value so
// that the set's loop keeps them in registers; on a miss, where the key
// would be.
static inline bool FindUnitAt(const uint8_t* bytes, uint32_t unitCount, uint32_t kind,
                              uint32_t locale, DataSpan* spanOut, uint32_t* atOut)
{
    uint64_t key = (uint64_t)kind << 32 | locale;
    uint32_t low = 0;
    uint32_t high = unitCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* entry = bytes + dataHeaderSize + (size_t)middle * dataUnitSize;
        uint64_t middleKey = (uint64_t)mlocLoad32(entry) << 32 | mlocLoad32(entry + 4);
        if (middleKey == key)
        {
            // An aligned payload between the tables and the pool.
            MLOC_ASSERT(mlocLoad32(entry + 8) % dataAlignment == 0 &&
                        mlocLoad32(entry + 8) >=
                            dataHeaderSize + (uint64_t)unitCount * dataUnitSize);
            *spanOut = (DataSpan){bytes + mlocLoad32(entry + 8), mlocLoad32(entry + 12)};
            return true;
        }
        if (middleKey < key)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    *atOut = low;
    return false;
}

static inline bool FindUnit(const uint8_t* bytes, uint32_t unitCount, uint32_t kind,
                            uint32_t locale, DataSpan* spanOut)
{
    uint32_t at = 0;
    return FindUnitAt(bytes, unitCount, kind, locale, spanOut, &at);
}

bool mlocFindDataUnit(const DataView* view, uint32_t kind, uint32_t locale, DataSpan* spanOut)
{
    return FindUnit(view->bytes, view->unitCount, kind, locale, spanOut);
}

bool mlocFindDataUnitOrKind(const DataView* view, uint32_t kind, uint32_t locale, DataSpan* spanOut,
                            bool* noKindOut)
{
    uint32_t at = 0;
    if (FindUnitAt(view->bytes, view->unitCount, kind, locale, spanOut, &at))
    {
        return true;
    }
    // The units sort by kind, then locale: a kind's units lie together,
    // so a miss between two units of other kinds means none.
    const uint8_t* units = view->bytes + dataHeaderSize;
    *noKindOut = (at == 0 || mlocLoad32(units + (size_t)(at - 1) * dataUnitSize) != kind) &&
                 (at == view->unitCount || mlocLoad32(units + (size_t)at * dataUnitSize) != kind);
    return false;
}

static inline bool HasKind(const DataView* view, uint32_t kind)
{
    // The first unit at or after the kind's first key.
    uint32_t low = 0;
    uint32_t high = view->unitCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* entry = view->bytes + dataHeaderSize + (size_t)middle * dataUnitSize;
        if (mlocLoad32(entry) < kind)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return low < view->unitCount &&
           mlocLoad32(view->bytes + dataHeaderSize + (size_t)low * dataUnitSize) == kind;
}

bool mlocDataHasKind(const DataView* view, uint32_t kind)
{
    return HasKind(view, kind);
}

bool mlocDataSetHasKind(const mlocDataSet* set, uint32_t kind)
{
    for (size_t i = 0; i < set->count; i++)
    {
        DataView view = mlocViewOf(&set->sources[i]);
        if (HasKind(&view, kind))
        {
            return true;
        }
    }
    return false;
}

// A unit for a locale id: the first source with the id and the unit.
static bool FindLocaleUnit(const mlocDataSet* set, uint32_t kind, const char* id, size_t length,
                           DataSpan* spanOut)
{
    for (size_t i = 0; i < set->count; i++)
    {
        DataView view = mlocViewOf(&set->sources[i]);
        uint32_t locale = dataNoLocale;
        if (mlocFindDataLocale(&view, id, length, &locale) &&
            FindUnit(view.bytes, view.unitCount, kind, locale, spanOut))
        {
            return true;
        }
    }
    return false;
}

bool mlocFindDataSetUnit(const mlocDataSet* set, uint32_t kind, const char* id, size_t length,
                         DataSpan* spanOut)
{
    if (id != nullptr)
    {
        return FindLocaleUnit(set, kind, id, length, spanOut);
    }
    // A unit of no locale, the common lookup, reads no locale table.
    for (size_t i = 0; i < set->count; i++)
    {
        DataView view = mlocViewOf(&set->sources[i]);
        if (FindUnit(view.bytes, view.unitCount, kind, dataNoLocale, spanOut))
        {
            return true;
        }
    }
    return false;
}
