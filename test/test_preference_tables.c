// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The regional preferences table's validator, with one field of a copy
// spoiled for each rule.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_prefs.h"
#include "locale_tables.h"
#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/locale.h"

#include <stdlib.h>
#include <string.h>

static DataSpan s_span;
static size_t s_regions;
static size_t s_locales;

static bool ValidWith(size_t offset, uint64_t value, int bytes)
{
    uint8_t* copy = malloc(s_span.size);
    if (copy == nullptr)
    {
        return true;
    }
    memcpy(copy, s_span.bytes, s_span.size);
    for (int i = 0; i < bytes; i++)
    {
        copy[offset + (size_t)i] = (uint8_t)(value >> (8 * i));
    }
    bool valid = mlocValidPreferences(nullptr, (DataSpan){copy, s_span.size});
    free(copy);
    return valid;
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    CHECK(mlocFindDataSetUnit(&data, kindLocalePreferences, nullptr, 0, &s_span),
          "the table is there");
    uint32_t hoursCount = mlocLoad16(s_span.bytes + 4);
    uint32_t regionCount = mlocLoad16(s_span.bytes);
    s_regions = preferHeaderSize + (size_t)hoursCount * preferEntrySize;
    s_locales = s_regions + (size_t)regionCount * preferEntrySize;
    size_t hours = preferHeaderSize;
    CHECK(mlocValidPreferences(nullptr, s_span), "the compiled-in table is valid");
    CHECK(!ValidWith(0, regionCount + 1, 2), "a count the size does not hold");
    CHECK(!ValidWith(6, 1, 2) && !ValidWith(8, 1, 8), "reserved header bytes");
    CHECK(!ValidWith(hours, mloc_hourFormatH24 + 1, 1), "a preferred format with a day period");
    CHECK(!ValidWith(hours + 1, 0, 1) && !ValidWith(hours + 1, preferHoursMax + 1, 1),
          "no allowed formats, or too many");
    CHECK(!ValidWith(hours + 2, mloc_hourFormatH12FlexibleDayPeriod + 1, 1), "an unknown format");
    CHECK(!ValidWith(hours + 2 + preferHoursMax - 1, 1, 1), "an unused format not zero");
    size_t r = s_regions + preferEntrySize;
    CHECK(!ValidWith(r, mlocLoad16(s_span.bytes + s_regions), 2), "regions out of order");
    CHECK(!ValidWith(s_regions, 0, 2), "a region no code is");
    CHECK(!ValidWith(r + 2, hoursCount, 1), "hours past the table");
    CHECK(!ValidWith(r + 3, 0x40, 1) && !ValidWith(r + 3, 0x48, 1) && !ValidWith(r + 3, 0x01, 1) &&
              !ValidWith(r + 3, 0x81, 1) && !ValidWith(r + 4, 0, 1) && !ValidWith(r + 5, 8, 1),
          "a day outside 1 to 7");
    CHECK(!ValidWith(r + 6, mloc_measurementUk + 1, 1), "an unknown measurement system");
    CHECK(!ValidWith(r + 6, (mloc_paperUsLetter + 1) << 4, 1), "an unknown paper size");
    CHECK(!ValidWith(r + 7, mloc_calendarDangi + 1, 1), "an unknown calendar");
    // 001 is the 1025th packed region; giving its entry another code
    // that keeps the order leaves the table without it.
    for (uint32_t i = 0; i < regionCount; i++)
    {
        size_t at = s_regions + (size_t)i * preferEntrySize;
        if (mlocLoad16(s_span.bytes + at) == mlocPackRegion("001", 3))
        {
            uint32_t before = i > 0 ? mlocLoad16(s_span.bytes + at - preferEntrySize) : 0;
            CHECK(before + 1 < mlocPackRegion("001", 3) && !ValidWith(at, before + 1, 2),
                  "a table without 001");
        }
    }
    size_t l = s_locales + preferEntrySize;
    CHECK(!ValidWith(l, mlocLoad32(s_span.bytes + s_locales), 4), "locales out of order");
    CHECK(!ValidWith(s_locales, 0, 2), "a language no code is");
    CHECK(!ValidWith(l + 4, hoursCount, 1), "a locale's hours past the table");
    CHECK(!ValidWith(l + 5, 1, 1) && !ValidWith(l + 6, 1, 2), "a locale's reserved bytes");
    CHECK(!mlocValidPreferences(nullptr, (DataSpan){s_span.bytes, 8}), "a header cut off");
    return s_failures == 0 ? 0 : 1;
}
