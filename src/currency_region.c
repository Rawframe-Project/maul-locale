// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The currencies of regions (record mloc-0022): a region's list in
// CLDR's order, and the currency a locale uses on a day, its -u-cu key's
// or its region's.

#include "currency_region.h"

#include "bytes.h"
#include "currency_tables.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_keyword.h"
#include "locale_prefs.h"
#include "locale_tables.h"

#include "maul-locale/currency.h"

#include <string.h>

typedef struct Regions
{
    const uint8_t* regions;
    const uint8_t* entries;
    uint32_t regionCount;
    uint32_t entryCount;
} Regions;

static Regions ReadRegions(DataSpan unit)
{
    Regions r = {.regionCount = mlocLoad16(unit.bytes), .entryCount = mlocLoad16(unit.bytes + 2)};
    r.regions = unit.bytes + currencyRegionsHeaderSize;
    r.entries = r.regions + (size_t)r.regionCount * currencyRegionSize;
    return r;
}

// A day of an entry, its two's complement bits (C23's only signed
// representation).
static int32_t LoadDay(const uint8_t* p)
{
    uint32_t value = mlocLoad32(p);
    int32_t day = 0;
    memcpy(&day, &value, sizeof(day));
    return day;
}

static bool ValidEntry(const uint8_t* e)
{
    uint32_t code = mlocLoad16(e + regionEntryCode);
    return code >= 1 && code <= currencyMaxCode && e[regionEntryTender] <= 1 && e[3] == 0 &&
           LoadDay(e + regionEntryFrom) <= LoadDay(e + regionEntryTo);
}

bool mlocValidCurrencyRegions(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < currencyRegionsHeaderSize)
    {
        return false;
    }
    Regions r = ReadRegions(payload);
    if (payload.size != currencyRegionsHeaderSize + (size_t)r.regionCount * currencyRegionSize +
                            (size_t)r.entryCount * currencyRegionEntrySize)
    {
        return false;
    }
    for (uint32_t i = 0; i < r.regionCount; i++)
    {
        const uint8_t* e = r.regions + (size_t)i * currencyRegionSize;
        uint32_t region = mlocLoad16(e);
        uint32_t first = mlocLoad16(e + 2);
        // Regions ascend; each has an entry, the first region's being 0.
        bool ordered = i == 0 ? first == 0
                              : region > mlocLoad16(e - currencyRegionSize) &&
                                    first > mlocLoad16(e - currencyRegionSize + 2);
        if (!ordered || first >= r.entryCount || !mlocValidPackedRegion(region))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < r.entryCount; i++)
    {
        if (!ValidEntry(r.entries + (size_t)i * currencyRegionEntrySize))
        {
            return false;
        }
    }
    return true;
}

// A region's entries: the first and the count, 0 for a region the unit
// lacks.
static uint32_t FindRegion(const Regions* r, uint32_t region, const uint8_t** firstOut)
{
    uint32_t low = 0;
    uint32_t high = r->regionCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* e = r->regions + (size_t)middle * currencyRegionSize;
        uint32_t at = mlocLoad16(e);
        if (at == region)
        {
            uint32_t first = mlocLoad16(e + 2);
            uint32_t end = middle + 1 < r->regionCount ? mlocLoad16(e + currencyRegionSize + 2)
                                                       : r->entryCount;
            *firstOut = r->entries + (size_t)first * currencyRegionEntrySize;
            return end - first;
        }
        if (at < region)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return 0;
}

static bool OpenRegions(const mlocDataSet* data, Regions* out)
{
    DataSpan unit;
    if (!mlocFindDataSetUnit(data, kindCurrencyRegions, nullptr, 0, &unit))
    {
        return false;
    }
    *out = ReadRegions(unit);
    return true;
}

static void WriteCode(uint16_t packed, char* out)
{
    mlocUnpackCurrency(packed, out);
    out[3] = '\0';
}

// The -u-cu key's code, or 0 when the locale has none of three letters.
static uint16_t KeyCurrency(const mlocLocale* locale)
{
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "cu", &value);
    return mlocPackCurrency(value, (uint32_t)length);
}

// The first legal tender in use on the day, else the first code in use,
// else 0.
static uint16_t RegionCurrency(const uint8_t* entries, uint32_t count, int32_t day)
{
    uint16_t other = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* e = entries + (size_t)i * currencyRegionEntrySize;
        if (LoadDay(e + regionEntryFrom) > day || LoadDay(e + regionEntryTo) < day)
        {
            continue;
        }
        uint16_t code = mlocLoad16(e + regionEntryCode);
        if (e[regionEntryTender] != 0)
        {
            return code;
        }
        other = other == 0 ? code : other;
    }
    return other;
}

mlocResult mlocFindCurrency(const mlocDataSet* data, const mlocLocale* locale, int32_t day,
                            char* codeOut)
{
    if (data == nullptr || locale == nullptr || codeOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    Regions r;
    if (!OpenRegions(data, &r))
    {
        return mloc_errorNotFound;
    }
    uint16_t code = KeyCurrency(locale);
    if (code == 0)
    {
        uint32_t language = 0;
        const uint8_t* entries = nullptr;
        uint32_t count = FindRegion(&r, mlocPreferenceRegion(data, locale, &language), &entries);
        code = RegionCurrency(entries, count, day);
    }
    if (code == 0)
    {
        return mloc_errorNotFound;
    }
    WriteCode(code, codeOut);
    return mloc_success;
}

mlocResult mlocGetRegionCurrencies(const mlocDataSet* data, const char* region, size_t regionLength,
                                   mlocRegionCurrency* currenciesOut, size_t capacity,
                                   size_t* countOut)
{
    uint32_t packed =
        region == nullptr || regionLength > 3 ? 0 : mlocPackRegion(region, (uint32_t)regionLength);
    if (data == nullptr || countOut == nullptr || packed == 0 ||
        (currenciesOut == nullptr && capacity > 0))
    {
        return mloc_errorInvalid;
    }
    Regions r;
    if (!OpenRegions(data, &r))
    {
        return mloc_errorNotFound;
    }
    const uint8_t* entries = nullptr;
    uint32_t count = FindRegion(&r, packed, &entries);
    *countOut = count;
    if (count > capacity)
    {
        return mloc_errorCapacity;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* e = entries + (size_t)i * currencyRegionEntrySize;
        mlocRegionCurrency c = {
            .from = LoadDay(e + regionEntryFrom),
            .to = LoadDay(e + regionEntryTo),
            .tender = e[regionEntryTender],
        };
        WriteCode(mlocLoad16(e + regionEntryCode), c.code);
        currenciesOut[i] = c;
    }
    return mloc_success;
}
