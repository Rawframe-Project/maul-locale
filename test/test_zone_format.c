// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone formatter (maul-locale/datetime.h) on the compiled-in data,
// zone-names.mloc and names.mloc: each style, the fallbacks to the
// location and the offset, generic names qualified by country or city,
// standard names for zones without daylight time, CLDR's daylight offsets
// over the zones unit's, offsets of seconds and other digits, parts,
// measuring, and the arguments refused. The texts are ICU4C 79's.

#define _CRT_SECURE_NO_WARNINGS

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"
#include "maul-locale/zone.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocData s_sources[3];
static mlocDataSet s_data;

static bool OpenFile(const char* path, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    uint8_t* bytes = malloc((size_t)8 << 20);
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)8 << 20, file);
    fclose(file);
    return read > 0 && mlocOpenData(out, bytes, read) == mloc_success;
}

enum
{
    // July 13 and January 9 of 2026, in July's daylight time and January's
    // standard time north of the equator.
    july = 1784000000,
    january = 1768000000,
};

static bool Make(const char* id, mlocZoneStyle style, mlocZoneFormatter* formatter)
{
    mlocLocale locale;
    return mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status ==
               mloc_success &&
           mlocInitZoneFormatter(formatter, &s_data, &locale, style) >= 0;
}

// Whether a locale writes a zone at an instant as want in a style.
static bool Is(const char* id, const char* zoneId, int64_t seconds, mlocZoneStyle style,
               const char* want)
{
    mlocZoneFormatter formatter;
    mlocTimeZone zone = {0};
    char text[256];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocInstant instant = {.seconds = seconds};
    if (!Make(id, style, &formatter) ||
        mlocFindTimeZone(&s_data, zoneId, strlen(zoneId), &zone) != mloc_success ||
        mlocFormatZone(&formatter, &zone, &instant, &output) != mloc_success)
    {
        printf("  %s %s: refused\n", id, zoneId);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0;
    if (!same)
    {
        printf("  %s %s: got \"%.*s\", want \"%s\"\n", id, zoneId, (int)output.length, text, want);
    }
    return same;
}

static void TestStyles(void)
{
    static const char* const s_want[] = {
        "",      "Pacific Daylight Time", "PDT",         "Pacific Time",
        "PT",    "Los Angeles Time",      "Los Angeles", "GMT-07:00",
        "GMT-7",
    };
    bool all = true;
    for (int style = mloc_zoneStyleSpecificLong; style <= mloc_zoneStyleOffsetShort; style++)
    {
        all = Is("en", "America/Los_Angeles", july, (mlocZoneStyle)style, s_want[style]) && all;
    }
    CHECK(all, "each style of Los Angeles in July");
    CHECK(Is("en", "America/Los_Angeles", january, mloc_zoneStyleSpecificLong,
             "Pacific Standard Time") &&
              Is("en", "America/Los_Angeles", january, mloc_zoneStyleOffsetShort, "GMT-8"),
          "and in January");
    CHECK(Is("de", "Asia/Kolkata", july, mloc_zoneStyleCity, "Kalkutta") &&
              Is("ja", "America/Vancouver", july, mloc_zoneStyleLocation,
                 "\xE3\x83\x90\xE3\x83\xB3\xE3\x82\xAF\xE3\x83\xBC\xE3\x83\x90\xE3\x83\xBC"
                 "\xE6\x99\x82\xE9\x96\x93"),
          "a localized city, a city's location");
    CHECK(Is("en", "Europe/Rome", july, mloc_zoneStyleLocation, "Italy Time") &&
              Is("en", "Asia/Shanghai", july, mloc_zoneStyleLocation, "China Time"),
          "a country's only zone and its primary zone named by the country");
}

static void TestFallbacks(void)
{
    CHECK(Is("en_GB", "America/Los_Angeles", july, mloc_zoneStyleSpecificShort, "GMT-7") &&
              Is("en_GB", "America/Los_Angeles", july, mloc_zoneStyleGenericShort,
                 "Los Angeles Time"),
          "no short names (en_001's \"\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85\"): offset, location");
    CHECK(
        Is("fr", "America/Los_Angeles", july, mloc_zoneStyleGenericShort, "heure : Los Angeles") &&
            Is("de", "America/Los_Angeles", july, mloc_zoneStyleGenericShort,
               "Los Angeles (Ortszeit)"),
        "the location in the locale's region format");
    CHECK(Is("en", "Africa/Casablanca", july, mloc_zoneStyleSpecificLong, "GMT+01:00") &&
              Is("en", "Africa/Casablanca", july, mloc_zoneStyleGenericLong, "Morocco Time"),
          "a zone in no metazone: the offset, else the location");
    CHECK(Is("en", "Etc/GMT", july, mloc_zoneStyleCity, "Unknown Location") &&
              Is("ja", "Etc/GMT", july, mloc_zoneStyleCity,
                 "\xE5\x9C\xB0\xE5\x9F\x9F\xE4\xB8\x8D\xE6\x98\x8E") &&
              Is("en", "Etc/GMT", july, mloc_zoneStyleLocation, "GMT+00:00"),
          "a zone of no region: the unknown city, the offset");
    CHECK(Is("en", "+05:30", july, mloc_zoneStyleGenericLong, "GMT+05:30") &&
              Is("en", "+05:30", july, mloc_zoneStyleCity, "Unknown Location") &&
              Is("en", "-03:00", july, mloc_zoneStyleSpecificShort, "GMT-3"),
          "an offset zone");
}

static void TestGeneric(void)
{
    CHECK(Is("en", "Asia/Kolkata", july, mloc_zoneStyleGenericLong, "India Standard Time") &&
              Is("en", "America/Phoenix", july, mloc_zoneStyleGenericLong,
                 "Mountain Standard Time") &&
              Is("en", "Asia/Kolkata", july, mloc_zoneStyleGenericShort, "India Time"),
          "standard time all around: the standard name, else the location");
    CHECK(Is("en_CA", "America/Los_Angeles", july, mloc_zoneStyleGenericShort, "PT") &&
              Is("en_AG", "America/Halifax", july, mloc_zoneStyleGenericLong,
                 "Atlantic Time (Canada)"),
          "qualified by the country where the region's reference zone differs");
    CHECK(Is("en", "America/Punta_Arenas", july, mloc_zoneStyleGenericLong,
             "Chile Time (Punta Arenas)") &&
              Is("en", "America/Punta_Arenas", january, mloc_zoneStyleGenericLong, "Chile Time"),
          "qualified by the city, as the offsets differ");
    CHECK(Is("en", "America/Indiana/Knox", 1143961200, mloc_zoneStyleGenericLong,
             "Central Time (Knox, Indiana)"),
          "the reference zone read at a skipped wall time by the offset before");
}

static void TestOffsets(void)
{
    CHECK(Is("en", "America/Punta_Arenas", july, mloc_zoneStyleSpecificLong, "Chile Summer Time") &&
              Is("en", "Europe/Dublin", july, mloc_zoneStyleSpecificLong, "Irish Standard Time"),
          "CLDR's daylight offset is daylight time");
    CHECK(Is("en", "Africa/Monrovia", 31536000, mloc_zoneStyleSpecificLong, "GMT-00:44:30") &&
              Is("en", "Africa/Monrovia", 31536000, mloc_zoneStyleOffsetShort, "GMT-0:44:30"),
          "an offset of seconds");
    CHECK(
        Is("fa", "Asia/Tehran", july, mloc_zoneStyleOffsetShort,
           "\xE2\x80\x8E+\xDB\xB3:\xDB\xB3\xDB\xB0 \xDA\xAF\xD8\xB1\xDB\x8C\xD9\x86\xD9\x88\xDB\x8C"
           "\xDA\x86") &&
            Is("fr", "Asia/Kolkata", july, mloc_zoneStyleOffsetLong, "UTC+05:30"),
        "the locale's digits and format");
}

static uint32_t s_partKind;
static size_t s_partStart;
static size_t s_partLength;
static int s_partCount;

static void OnPart(void* context, uint32_t kind, size_t start, size_t length)
{
    (void)context;
    s_partKind = kind;
    s_partStart = start;
    s_partLength = length;
    s_partCount++;
}

static void TestOutput(void)
{
    mlocZoneFormatter formatter;
    mlocTimeZone zone = {0};
    mlocInstant instant = {.seconds = july};
    char text[64];
    CHECK(Make("en", mloc_zoneStyleGenericLong, &formatter) &&
              mlocFindTimeZone(&s_data, "America/Los_Angeles", 19, &zone) == mloc_success,
          "a formatter and a zone");
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = OnPart};
    CHECK(mlocFormatZone(&formatter, &zone, &instant, &output) == mloc_success &&
              s_partCount == 1 && s_partKind == mloc_datePartTimeZone && s_partStart == 0 &&
              s_partLength == 12,
          "one part, the zone");
    mlocOutput small = {.buffer = text, .capacity = 4};
    CHECK(mlocFormatZone(&formatter, &zone, &instant, &small) == mloc_errorCapacity &&
              small.length == 12,
          "too small a buffer: the size needed");
    mlocOutput measure = {0};
    CHECK(mlocFormatZone(&formatter, &zone, &instant, &measure) == mloc_errorCapacity &&
              measure.length == 12,
          "measuring");
}

static void TestRefused(void)
{
    mlocLocale en;
    mlocZoneFormatter formatter;
    mlocTimeZone zone = {0};
    mlocInstant instant = {.seconds = july};
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocFindTimeZone(&s_data, "Europe/Paris", 12, &zone) == mloc_success,
          "en and Paris");
    CHECK(
        mlocInitZoneFormatter(&formatter, &s_data, &en, mloc_zoneStyleNone) == mloc_errorInvalid &&
            mlocInitZoneFormatter(&formatter, &s_data, &en, (mlocZoneStyle)9) ==
                mloc_errorInvalid &&
            mlocInitZoneFormatter(nullptr, &s_data, &en, mloc_zoneStyleCity) == mloc_errorInvalid &&
            mlocInitZoneFormatter(&formatter, nullptr, &en, mloc_zoneStyleCity) ==
                mloc_errorInvalid &&
            mlocInitZoneFormatter(&formatter, &s_data, nullptr, mloc_zoneStyleCity) ==
                mloc_errorInvalid,
        "no style, a style out of range, null pointers");
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocInitZoneFormatter(&formatter, &builtin, &en, mloc_zoneStyleCity) ==
              mloc_errorNotFound,
          "data without the zone names");
    CHECK(mlocInitZoneFormatter(&formatter, &s_data, &en, mloc_zoneStyleCity) == mloc_success,
          "a formatter");
    mlocInstant far = {.seconds = INT64_C(8640000000001)};
    mlocInstant fraction = {.seconds = 0, .nanoseconds = 1000000000};
    mlocTimeZone forged = zone;
    forged.offsetMinutes = 30;
    CHECK(mlocFormatZone(nullptr, &zone, &instant, &output) == mloc_errorInvalid &&
              mlocFormatZone(&formatter, nullptr, &instant, &output) == mloc_errorInvalid &&
              mlocFormatZone(&formatter, &zone, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatZone(&formatter, &zone, &instant, nullptr) == mloc_errorInvalid &&
              mlocFormatZone(&formatter, &zone, &far, &output) == mloc_errorInvalid &&
              mlocFormatZone(&formatter, &zone, &fraction, &output) == mloc_errorInvalid &&
              mlocFormatZone(&formatter, &forged, &instant, &output) == mloc_errorInvalid,
          "null pointers, an instant out of range, a zone the library did not make");
    mlocOutput noBuffer = {.capacity = 4};
    CHECK(mlocFormatZone(&formatter, &zone, &instant, &noBuffer) == mloc_errorInvalid,
          "a capacity without a buffer");
    mlocZoneFormatter spoiled = formatter;
    memset(&spoiled, 0, sizeof(spoiled));
    CHECK(mlocFormatZone(&spoiled, &zone, &instant, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 3};
    if (!OpenFile(MLOC_ZONE_NAMES_FILE, &s_sources[1]) ||
        !OpenFile(MLOC_LOCALE_NAMES_FILE, &s_sources[2]))
    {
        fputs("test_zone_format: cannot open the data files\n", stderr);
        return 2;
    }
    TestStyles();
    TestFallbacks();
    TestGeneric();
    TestOffsets();
    TestOutput();
    TestRefused();
    return s_failures == 0 ? 0 : 1;
}
