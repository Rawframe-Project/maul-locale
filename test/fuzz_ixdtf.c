// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the parsers of Temporal's strings (record
// mloc-0041). Every parser reads the input; what one accepts must hold:
// a date-time's spans lie inside the input, its fields in their ranges,
// and a duration's fields are integers of one sign that make a valid
// record again.

#include "maul-locale/ixdtf.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void RequireInside(mlocTextSpan span, size_t size)
{
    Require(span.start <= size && span.length <= size - span.start);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const char* text = (const char*)data;
    for (uint32_t formats = 1; formats <= 63; formats += formats < 32 ? formats : 31)
    {
        mlocTemporalParse p;
        if (mlocParseTemporalString(text, size, formats, &p) == mloc_success)
        {
            Require((p.format & formats) != 0 && p.month >= 1 && p.month <= 12 && p.day >= 1 &&
                    p.day <= 31 && p.time.hour < 24 && p.time.minute < 60 && p.time.second < 60 &&
                    p.time.nanosecond < 1000000000);
            Require(p.offsetNanoseconds > -INT64_C(86400000000000) &&
                    p.offsetNanoseconds < INT64_C(86400000000000));
            RequireInside(p.timeZone, size);
            RequireInside(p.calendar, size);
        }
    }
    int64_t offset = 0;
    if (mlocParseUTCOffset(text, size, &offset, nullptr) == mloc_success)
    {
        Require(offset > -INT64_C(86400000000000) && offset < INT64_C(86400000000000));
    }
    mlocTimeZoneParse zone;
    if (mlocParseTimeZoneString(text, size, &zone) == mloc_success)
    {
        RequireInside(zone.name, size);
        Require(zone.offsetMinutes > -1440 && zone.offsetMinutes < 1440);
    }
    mlocTextSpan calendar;
    if (mlocParseCalendarString(text, size, &calendar) == mloc_success)
    {
        RequireInside(calendar, size);
    }
    mlocDurationValues values;
    mlocDateTimeDuration record;
    if (mlocParseDuration(text, size, &values) == mloc_success)
    {
        Require(mlocMakeDuration(&values, &record) == mloc_success);
    }
    return 0;
}
