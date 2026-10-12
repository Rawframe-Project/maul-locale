// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Regional preferences (UTS #35 Parts 1 and 4): the region a locale's
// preferences come from, the -u keys that override them, and the
// table's lookups. The table's layout is in locale_tables.h.

#include "locale_prefs.h"

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_keyword.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

typedef struct Prefs
{
    const uint8_t* hours;
    const uint8_t* regions;
    const uint8_t* locales;
    uint32_t hoursCount;
    uint32_t regionCount;
    uint32_t localeCount;
} Prefs;

static bool ReadPrefs(DataSpan span, Prefs* p)
{
    if (span.size < preferHeaderSize)
    {
        return false;
    }
    *p = (Prefs){.regionCount = mlocLoad16(span.bytes),
                 .localeCount = mlocLoad16(span.bytes + 2),
                 .hoursCount = mlocLoad16(span.bytes + 4)};
    p->hours = span.bytes + preferHeaderSize;
    p->regions = p->hours + (size_t)p->hoursCount * preferEntrySize;
    p->locales = p->regions + (size_t)p->regionCount * preferEntrySize;
    return preferHeaderSize +
               ((size_t)p->hoursCount + p->regionCount + p->localeCount) * preferEntrySize ==
           span.size;
}

static bool ValidDay(uint8_t day)
{
    return day >= 1 && day <= 7;
}

static bool ValidRegions(const Prefs* p)
{
    bool world = false;
    for (uint32_t i = 0; i < p->regionCount; i++)
    {
        const uint8_t* e = p->regions + (size_t)i * preferEntrySize;
        uint32_t region = mlocLoad16(e);
        if ((i > 0 && region <= mlocLoad16(e - preferEntrySize)) ||
            !mlocValidPackedRegion(region) || e[2] >= p->hoursCount || !ValidDay(e[3]) ||
            !ValidDay(e[4]) || !ValidDay(e[5]) || !ValidDay(e[6]) ||
            (e[7] & 15) > mloc_measurementUk || e[7] >> 4 > mloc_paperUsLetter)
        {
            return false;
        }
        world |= region == mlocPackRegion("001", 3);
    }
    return world;
}

bool mlocValidPreferences(const DataView* view, DataSpan payload)
{
    (void)view;
    Prefs p;
    if (!ReadPrefs(payload, &p) || mlocLoad16(payload.bytes + 6) != 0 ||
        mlocLoad64(payload.bytes + 8) != 0)
    {
        return false;
    }
    for (uint32_t i = 0; i < p.hoursCount; i++)
    {
        const uint8_t* e = p.hours + (size_t)i * preferEntrySize;
        if (e[0] > mloc_hourFormatH24 || e[1] == 0 || e[1] > preferHoursMax)
        {
            return false;
        }
        for (uint32_t k = 0; k < preferHoursMax; k++)
        {
            if (k < e[1] ? e[2 + k] > mloc_hourFormatH12FlexibleDayPeriod : e[2 + k] != 0)
            {
                return false;
            }
        }
    }
    for (uint32_t i = 0; i < p.localeCount; i++)
    {
        const uint8_t* e = p.locales + (size_t)i * preferEntrySize;
        uint32_t key = mlocLoad32(e);
        if ((i > 0 && key <= mlocLoad32(e - preferEntrySize)) ||
            !mlocValidPackedLanguage(mlocLoad16(e)) || !mlocValidPackedRegion(mlocLoad16(e + 2)) ||
            e[4] >= p.hoursCount || e[5] != 0 || mlocLoad16(e + 6) != 0)
        {
            return false;
        }
    }
    return ValidRegions(&p);
}

// The entry of a region, else 001's.
static const uint8_t* RegionEntry(const Prefs* p, uint32_t region)
{
    uint32_t low = 0;
    uint32_t high = p->regionCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t at = mlocLoad16(p->regions + (size_t)middle * preferEntrySize);
        if (at == region)
        {
            return p->regions + (size_t)middle * preferEntrySize;
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
    uint32_t world = mlocPackRegion("001", 3);
    return region == world ? nullptr : RegionEntry(p, world);
}

// The hours of a language in a region, or null.
static const uint8_t* LocaleHours(const Prefs* p, uint32_t language, uint32_t region)
{
    uint32_t key = region << 16 | language;
    for (uint32_t i = 0; language != 0 && i < p->localeCount; i++)
    {
        if (mlocLoad32(p->locales + (size_t)i * preferEntrySize) == key)
        {
            return p->hours + (size_t)p->locales[(size_t)i * preferEntrySize + 4] * preferEntrySize;
        }
    }
    return nullptr;
}

static bool Is(const char* value, size_t length, const char* text)
{
    return strlen(text) == length && memcmp(value, text, length) == 0;
}

// The region of an rg or sd value: a region of two letters with a
// subdivision suffix ("uszzzz", "gbsct"), else 0.
static uint32_t SubdivisionRegion(const mlocLocale* locale, const char* key)
{
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, key, &value);
    if (length < 3 || length > 6 || value[0] < 'a' || value[0] > 'z' || value[1] < 'a' ||
        value[1] > 'z')
    {
        return 0;
    }
    return mlocPackRegion(value, 2);
}

static uint32_t FieldLength(const char* field, uint32_t size)
{
    uint32_t n = 0;
    while (n < size && field[n] != '\0')
    {
        n++;
    }
    return n;
}

uint32_t mlocPreferenceRegion(const mlocDataSet* data, const mlocLocale* locale,
                              uint32_t* languageOut)
{
    mlocLocale max;
    bool maximized = mlocMaximizeLocale(data, locale, &max) == mloc_success;
    const mlocLocale* named = maximized ? &max : locale;
    *languageOut = mlocPackLanguage(named->language, FieldLength(named->language, 8));
    uint32_t region = SubdivisionRegion(locale, "rg");
    if (region == 0)
    {
        region = mlocPackRegion(locale->region, FieldLength(locale->region, 4));
    }
    if (region == 0)
    {
        region = SubdivisionRegion(locale, "sd");
    }
    if (region == 0 && maximized)
    {
        region = mlocPackRegion(max.region, FieldLength(max.region, 4));
    }
    return region != 0 ? region : mlocPackRegion("001", 3);
}

// The hour cycle of -u-hc, or the preferred one: c12 and c24 take the
// first of the allowed formats with that many hours.
static mlocHourFormat HourCycle(const mlocLocale* locale, const uint8_t* hours)
{
    static const char* const s_cycles[4] = {"h11", "h12", "h23", "h24"};
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "hc", &value);
    for (uint8_t c = 0; c < 4; c++)
    {
        if (Is(value, length, s_cycles[c]))
        {
            return c;
        }
    }
    bool twelve = Is(value, length, "c12");
    if (twelve || Is(value, length, "c24"))
    {
        // K before h gives h11, else h12; k before H gives h24, else h23.
        mlocHourFormat other = twelve ? mloc_hourFormatH11 : mloc_hourFormatH24;
        mlocHourFormat usual = twelve ? mloc_hourFormatH12 : mloc_hourFormatH23;
        for (uint8_t i = 0; i < hours[1]; i++)
        {
            uint8_t f = hours[2 + i] == mloc_hourFormatH12DayPeriod ||
                                hours[2 + i] == mloc_hourFormatH12FlexibleDayPeriod
                            ? mloc_hourFormatH12
                            : hours[2 + i];
            if (f == other || f == usual)
            {
                return f;
            }
        }
        return usual;
    }
    return hours[0];
}

// The first day of -u-fw, else the region override's, else Monday for
// the ISO 8601 calendar, else the region's.
static uint8_t FirstDay(const mlocLocale* locale, const uint8_t* region, bool iso)
{
    static const char* const s_days[7] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "fw", &value);
    for (uint8_t d = 0; d < 7; d++)
    {
        if (Is(value, length, s_days[d]))
        {
            return (uint8_t)(d + 1);
        }
    }
    return iso && SubdivisionRegion(locale, "rg") == 0 ? 1 : region[3];
}

static mlocMeasurementSystem MeasurementSystem(const mlocLocale* locale, const uint8_t* region)
{
    static const char* const s_systems[3] = {"metric", "ussystem", "uksystem"};
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "ms", &value);
    for (uint8_t m = 0; m < 3; m++)
    {
        if (Is(value, length, s_systems[m]))
        {
            return m;
        }
    }
    return region[7] & 15;
}

mlocResult mlocGetLocalePreferences(const mlocDataSet* data, const mlocLocale* locale,
                                    mlocLocalePreferences* preferencesOut)
{
    if (data == nullptr || locale == nullptr || preferencesOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    DataSpan span;
    Prefs p;
    if (!mlocFindDataSetUnit(data, kindLocalePreferences, nullptr, 0, &span) ||
        !ReadPrefs(span, &p))
    {
        return mloc_errorNotFound;
    }
    uint32_t language = 0;
    uint32_t region = mlocPreferenceRegion(data, locale, &language);
    const uint8_t* entry = RegionEntry(&p, region);
    const uint8_t* hours = LocaleHours(&p, language, region);
    if (hours == nullptr)
    {
        hours = p.hours + (size_t)entry[2] * preferEntrySize;
    }
    const char* calendar = nullptr;
    size_t calendarLength = mlocFindKeyword(locale, "ca", &calendar);
    bool iso = Is(calendar, calendarLength, "iso8601");
    mlocLocalePreferences out = {
        .hourCycle = HourCycle(locale, hours),
        .allowedHourCount = hours[1],
        .firstDay = FirstDay(locale, entry, iso),
        .minimalDays = iso ? 4 : entry[4],
        .weekendStart = entry[5],
        .weekendEnd = entry[6],
        .measurementSystem = MeasurementSystem(locale, entry),
        .paperSize = (mlocPaperSize)(entry[7] >> 4),
    };
    mlocUnpackRegion(region, out.region);
    memcpy(out.allowedHours, hours + 2, preferHoursMax);
    *preferencesOut = out;
    return mloc_success;
}
