// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the zone names data file's validators. The input's
// first byte picks the unit (even: metazones, odd: zone names), the rest
// is its payload; one the validator accepts must keep every read inside
// its own bytes: each code's periods ascending, each metazone's golden and
// preferred zones among the codes, each name's key a field of its item and
// its text inside the text.

#include "bytes.h"
#include "zone_name_tables.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void CheckMeta(DataSpan span)
{
    uint32_t codeCount = mlocLoad32(span.bytes);
    uint32_t periodCount = mlocLoad32(span.bytes + 4);
    uint32_t metazoneCount = mlocLoad32(span.bytes + 8);
    uint32_t preferredCount = mlocLoad32(span.bytes + 12);
    const uint8_t* codes = span.bytes + zoneMetaHeaderSize;
    const uint8_t* periods = codes + (size_t)codeCount * zoneMetaCodeSize;
    const uint8_t* metazones = periods + (size_t)periodCount * zoneMetaPeriodSize;
    const uint8_t* preferred = metazones + (size_t)metazoneCount * zoneMetaZoneSize;
    const uint8_t* end = preferred + (size_t)preferredCount * zoneMetaPreferredSize;
    Require(end == span.bytes + span.size);
    for (uint32_t i = 0; i < codeCount; i++)
    {
        const uint8_t* c = codes + (size_t)i * zoneMetaCodeSize;
        int64_t previous = INT64_MIN;
        for (uint32_t k = 0; k < c[3]; k++)
        {
            const uint8_t* p = periods + (size_t)(mlocLoad16(c + 4) + k) * zoneMetaPeriodSize;
            Require(p + zoneMetaPeriodSize <= metazones);
            Require((int64_t)mlocLoad64(p) >= previous);
            Require(mlocLoad16(p + 16) < metazoneCount);
            previous = (int64_t)mlocLoad64(p + 8);
        }
    }
    for (uint32_t i = 0; i < metazoneCount; i++)
    {
        const uint8_t* m = metazones + (size_t)i * zoneMetaZoneSize;
        Require(mlocLoad16(m) < codeCount);
        for (uint32_t k = 0; k < mlocLoad16(m + 4); k++)
        {
            const uint8_t* p = preferred + (size_t)(mlocLoad16(m + 2) + k) * zoneMetaPreferredSize;
            Require(p + zoneMetaPreferredSize <= end && mlocLoad16(p + 2) < codeCount);
        }
    }
}

static void CheckNames(DataSpan span)
{
    uint32_t count = mlocLoad32(span.bytes);
    uint32_t textSize = mlocLoad32(span.bytes + 4);
    const uint8_t* keys = span.bytes + zoneNameHeaderSize;
    const uint8_t* offsets = keys + 2 * (size_t)count;
    const uint8_t* text = offsets + 4 * ((size_t)count + 1);
    Require(text + textSize == span.bytes + span.size);
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t key = mlocLoad16(keys + 2 * (size_t)i);
        uint32_t item = key / zoneNameFieldsPerItem;
        uint32_t field = key % zoneNameFieldsPerItem;
        Require(item == zoneNameFormats ? field < zoneFormatCount : field < zoneFieldCount);
        Require(item < zoneNameMetazoneBase || field != zoneFieldCity);
        Require(mlocLoad32(offsets + 4 * (size_t)i) <= mlocLoad32(offsets + 4 * (size_t)i + 4));
        Require(mlocLoad32(offsets + 4 * (size_t)i + 4) <= textSize);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Aligned, as a payload in a data file is.
    static uint64_t s_bytes[1024];
    if (size == 0 || size - 1 > sizeof(s_bytes))
    {
        return 0;
    }
    memcpy(s_bytes, data + 1, size - 1);
    DataSpan span = {(const uint8_t*)s_bytes, (uint32_t)(size - 1)};
    DataView view = {0};
    if (data[0] % 2 == 0 && mlocValidZoneMeta(&view, span))
    {
        CheckMeta(span);
    }
    else if (data[0] % 2 == 1 && mlocValidZoneNames(&view, span))
    {
        CheckNames(span);
    }
    return 0;
}
