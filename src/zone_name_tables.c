// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone names data file's units (zone_name_tables.h): their
// validators. A unit is checked alone; the code and metazone indexes a
// zone names unit holds are bounded by its keys, and the formatter checks
// them against the metazone unit it reads beside it.

#include "zone_name_tables.h"

#include "bytes.h"
#include "number_data.h"

enum
{
    // The largest offset CLDR gives a metazone period, in minutes.
    maxOffsetMinutes = 18 * 60,
};

static bool ValidRegion(const uint8_t* region, bool none)
{
    bool letters = region[0] >= 'A' && region[0] <= 'Z' && region[1] >= 'A' && region[1] <= 'Z';
    return letters || (none && region[0] == 0 && region[1] == 0);
}

static bool ValidOffset(int32_t minutes)
{
    return minutes >= -maxOffsetMinutes && minutes <= maxOffsetMinutes;
}

static bool ValidPeriod(const uint8_t* p, uint32_t metazoneCount)
{
    int64_t from = (int64_t)mlocLoad64(p);
    int64_t to = (int64_t)mlocLoad64(p + 8);
    uint8_t flags = p[18];
    int32_t standard = (int16_t)mlocLoad16(p + 20);
    int32_t daylight = (int16_t)mlocLoad16(p + 22);
    bool offsets = flags == zoneMetaOffsets ? ValidOffset(standard) && ValidOffset(daylight)
                                            : flags == 0 && standard == 0 && daylight == 0;
    return from < to && mlocLoad16(p + 16) < metazoneCount && p[19] == 0 && offsets;
}

// Each code's region, flags and periods, ascending without overlap.
static bool ValidCodes(const uint8_t* codes, uint32_t codeCount, const uint8_t* periods,
                       uint32_t periodCount, uint32_t metazoneCount)
{
    for (uint32_t i = 0; i < codeCount; i++)
    {
        const uint8_t* c = codes + (size_t)i * zoneMetaCodeSize;
        uint32_t count = c[3];
        uint32_t first = mlocLoad16(c + 4);
        if (!ValidRegion(c, true) || c[2] > zoneMetaCountry || mlocLoad16(c + 6) != 0 ||
            (count == 0 && first != 0) || first + count > periodCount)
        {
            return false;
        }
        for (uint32_t k = 0; k < count; k++)
        {
            const uint8_t* p = periods + (size_t)(first + k) * zoneMetaPeriodSize;
            if (!ValidPeriod(p, metazoneCount) ||
                (k > 0 && (int64_t)mlocLoad64(p - zoneMetaPeriodSize + 8) > (int64_t)mlocLoad64(p)))
            {
                return false;
            }
        }
    }
    return true;
}

// Each metazone's golden zone and preferred zones, ascending by region.
static bool ValidMetazones(const uint8_t* metazones, uint32_t metazoneCount,
                           const uint8_t* preferred, uint32_t preferredCount, uint32_t codeCount)
{
    for (uint32_t i = 0; i < metazoneCount; i++)
    {
        const uint8_t* m = metazones + (size_t)i * zoneMetaZoneSize;
        uint32_t first = mlocLoad16(m + 2);
        uint32_t count = mlocLoad16(m + 4);
        if (mlocLoad16(m) >= codeCount || mlocLoad16(m + 6) != 0 || (count == 0 && first != 0) ||
            first + count > preferredCount)
        {
            return false;
        }
        for (uint32_t k = 0; k < count; k++)
        {
            const uint8_t* p = preferred + (size_t)(first + k) * zoneMetaPreferredSize;
            if (!ValidRegion(p, false) || mlocLoad16(p + 2) >= codeCount ||
                (k > 0 && memcmp(p - zoneMetaPreferredSize, p, 2) >= 0))
            {
                return false;
            }
        }
    }
    return true;
}

bool mlocValidZoneMeta(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < zoneMetaHeaderSize)
    {
        return false;
    }
    uint32_t codeCount = mlocLoad32(payload.bytes);
    uint32_t periodCount = mlocLoad32(payload.bytes + 4);
    uint32_t metazoneCount = mlocLoad32(payload.bytes + 8);
    uint32_t preferredCount = mlocLoad32(payload.bytes + 12);
    uint64_t size = zoneMetaHeaderSize + (uint64_t)codeCount * zoneMetaCodeSize +
                    (uint64_t)periodCount * zoneMetaPeriodSize +
                    (uint64_t)metazoneCount * zoneMetaZoneSize +
                    (uint64_t)preferredCount * zoneMetaPreferredSize;
    if (codeCount > zoneNameMaxCodes || metazoneCount > zoneNameMaxMetazones ||
        periodCount > UINT16_MAX || preferredCount > UINT16_MAX || size != payload.size)
    {
        return false;
    }
    const uint8_t* codes = payload.bytes + zoneMetaHeaderSize;
    const uint8_t* periods = codes + (size_t)codeCount * zoneMetaCodeSize;
    const uint8_t* metazones = periods + (size_t)periodCount * zoneMetaPeriodSize;
    const uint8_t* preferred = metazones + (size_t)metazoneCount * zoneMetaZoneSize;
    return ValidCodes(codes, codeCount, periods, periodCount, metazoneCount) &&
           ValidMetazones(metazones, metazoneCount, preferred, preferredCount, codeCount);
}

// Whether a key names an item's field the layout has.
static bool ValidKey(uint32_t key)
{
    uint32_t item = key / zoneNameFieldsPerItem;
    uint32_t field = key % zoneNameFieldsPerItem;
    // The formats hold eight fields, every field a key can name.
    if (item == zoneNameFormats)
    {
        return true;
    }
    if (item < zoneNameMetazoneBase)
    {
        return field < zoneFieldCount;
    }
    return item < zoneNameMetazoneBase + zoneNameMaxMetazones && field != zoneFieldCity &&
           field < zoneFieldCount;
}

bool mlocValidZoneNames(const DataView* view, DataSpan payload)
{
    (void)view;
    // The header and the one offset of a unit without entries.
    if (payload.size < zoneNameHeaderSize + 4)
    {
        return false;
    }
    uint32_t count = mlocLoad32(payload.bytes);
    uint32_t textSize = mlocLoad32(payload.bytes + 4);
    uint64_t size = zoneNameHeaderSize + 2 * (uint64_t)count + 4 * ((uint64_t)count + 1) + textSize;
    if (size != payload.size)
    {
        return false;
    }
    const uint8_t* keys = payload.bytes + zoneNameHeaderSize;
    const uint8_t* offsets = keys + 2 * (size_t)count;
    const uint8_t* text = offsets + 4 * ((size_t)count + 1);
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * (size_t)count) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t key = mlocLoad16(keys + 2 * (size_t)i);
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        if (!ValidKey(key) || (i > 0 && key <= mlocLoad16(keys + 2 * (size_t)i - 2)) ||
            end < start || end > textSize || !mlocValidDataText(text + start, end - start, 0))
        {
            return false;
        }
    }
    return true;
}
