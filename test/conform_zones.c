// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Prints every zone's offsets from the compiled-in zones unit, or from a
// zones file put before it, over a span of years, for
// tools/check_zones.py to compare with zdump on zic's output for the same
// tzdb source: "release NAME" first, then "zone NAME STANDARD SAVE" with
// the offset at the span's start, then "at SECONDS STANDARD SAVE" for each
// transition in the span.
//
// usage: conform_zones <first year> <end year> [zones file]

// fopen, which MSVC's runtime deprecates.
#define _CRT_SECURE_NO_WARNINGS

#include "calendar_arith.h"
#include "zone_rules.h"

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <stdio.h>
#include <stdlib.h>

// Reads a whole file into memory from malloc, 8-byte aligned; null on
// failure.
static void* ReadFile(const char* path, size_t* sizeOut)
{
    FILE* file = fopen(path, "rb");
    void* bytes = nullptr;
    long size = -1;
    if (file != nullptr && fseek(file, 0, SEEK_END) == 0)
    {
        size = ftell(file);
    }
    if (size > 0 && fseek(file, 0, SEEK_SET) == 0)
    {
        bytes = malloc((size_t)size);
    }
    if (bytes != nullptr && fread(bytes, 1, (size_t)size, file) != (size_t)size)
    {
        free(bytes);
        bytes = nullptr;
    }
    if (file != nullptr)
    {
        fclose(file);
    }
    *sizeOut = bytes != nullptr ? (size_t)size : 0;
    return bytes;
}

static int64_t YearStart(int64_t year)
{
    // Day 719163 of the library's count is January 1 of 1970.
    return (mlocArithmeticToDay(mloc_calendarGregorian, year, 1, 1) - 719163) * 86400;
}

int main(int argc, char** argv)
{
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[1]);
    mlocDataSet data = {&sources[1], 1};
    size_t size = 0;
    void* file = argc == 4 ? ReadFile(argv[3], &size) : nullptr;
    if (file != nullptr && mlocOpenData(&sources[0], file, size) == mloc_success)
    {
        data = (mlocDataSet){sources, 2};
    }
    ZoneTables t;
    char release[16];
    mlocOutput out = {.buffer = release, .capacity = sizeof release};
    if ((argc != 3 && (argc != 4 || data.count != 2)) || !mlocFindZoneTables(&data, &t) ||
        mlocGetTimeZoneRelease(&data, &out) != mloc_success)
    {
        fputs("usage: conform_zones <first year> <end year> [zones file]\n", stderr);
        free(file);
        return 2;
    }
    printf("release %.*s\n", (int)out.length, release);
    int64_t from = YearStart(atoi(argv[1]));
    int64_t end = YearStart(atoi(argv[2]));
    from = from < zoneMinSeconds ? zoneMinSeconds : from;
    for (uint32_t z = 0; z < t.zoneCount; z++)
    {
        size_t length = 0;
        const char* name = mlocZoneIdentifierName(&t, mlocZoneRecord(&t, z).name, &length);
        ZoneOffset o = mlocZoneOffsetAt(&t, z, from);
        printf("zone %.*s %d %d\n", (int)length, name, (int)o.standard, (int)o.save);
        int64_t at = from;
        while (mlocZoneNextTransition(&t, z, at, &at, &o) && at < end)
        {
            printf("at %lld %d %d\n", (long long)at, (int)o.standard, (int)o.save);
        }
    }
    free(file);
    return 0;
}
