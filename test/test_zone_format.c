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

static mlocData s_sources[4];
static mlocDataSet s_data;

static bool OpenFile(const char* path, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    // Each file's own size: four files of 8 MiB outgrow the web build's
    // memory.
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    uint8_t* bytes = size > 0 && fseek(file, 0, SEEK_SET) == 0 ? malloc((size_t)size) : nullptr;
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)size, file);
    fclose(file);
    if (read > 0 && mlocOpenData(bytes, read, out) == mloc_success)
    {
        return true; // the bytes stay for the run
    }
    free(bytes);
    return false;
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
           mlocInitZoneFormatter(&s_data, &locale, style, formatter) >= 0;
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
        mlocZoneFormatter_Format(&formatter, &zone, &instant, &output) != mloc_success)
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
    CHECK(Is("en", "Africa/Algiers", 309740399, mloc_zoneStyleGenericLong,
             "Central European Standard Time") &&
              Is("en", "Africa/Algiers", 246236400, mloc_zoneStyleGenericLong,
                 "Central European Time") &&
              Is("en", "Africa/Algiers", 357523199, mloc_zoneStyleGenericLong,
                 "Western European Time (Algiers)"),
          "the standard name only with no daylight time within 184 days either side");
    CHECK(Is("en", "Africa/Casablanca", 1540691999, mloc_zoneStyleGenericLong,
             "Western European Time (Casablanca)"),
          "qualified where the offset changes at the instant");
    CHECK(Is("en_GB", "Africa/Algiers", july, mloc_zoneStyleGenericShort, "CET (Algiers)") &&
              Is("de", "Africa/Algiers", july, mloc_zoneStyleGenericShort, "MEZ (Algier)"),
          "a standard name equal to the generic one ignoring case is not taken");
    CHECK(Is("ja", "Africa/Algiers", 246236399, mloc_zoneStyleGenericLong,
             "\xE8\xA5\xBF\xE3\x83\xA8\xE3\x83\xBC\xE3\x83\xAD\xE3\x83\x83\xE3\x83\x91"
             "\xE6\x99\x82\xE9\x96\x93\xEF\xBC\x88\xE3\x82\xA2\xE3\x83\xAB\xE3\x82\xB8"
             "\xE3\x82\xA7\xEF\xBC\x89") &&
              Is("ar", "Africa/Tripoli", 875916000, mloc_zoneStyleGenericLong,
                 "\xD8\xAA\xD9\x88\xD9\x82\xD9\x8A\xD8\xAA \xD8\xB4\xD8\xB1\xD9\x82 "
                 "\xD8\xA3\xD9\x88\xD8\xB1\xD9\x88\xD8\xA8\xD8\xA7"),
          "the locale's fallback format; a locale without a region by its likely one");
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

// Cases found by the mutants, as ICU4C 79 writes them.
static void TestEdges(void)
{
    CHECK(Is("en", "Europe/Andorra", july, mloc_zoneStyleSpecificLong,
             "Central European Summer Time") &&
              Is("en", "Europe/Andorra", july, mloc_zoneStyleGenericShort, "Andorra Time") &&
              Is("en", "Europe/Andorra", july, mloc_zoneStyleCity, "Andorra"),
          "the first zone code (Andorra's)");
    CHECK(Is("en", "America/Eirunepe", july, mloc_zoneStyleSpecificLong, "Acre Standard Time") &&
              Is("en", "America/Eirunepe", july, mloc_zoneStyleGenericLong, "Acre Standard Time"),
          "the first metazone (Acre)");
    CHECK(Is("en", "America/Adak", january, mloc_zoneStyleOffsetShort, "GMT-10"),
          "an offset of ten hours");
    CHECK(Is("en-u-nu-mathbold", "America/Los_Angeles", july, mloc_zoneStyleOffsetShort,
             "GMT-\xF0\x9D\x9F\x95"),
          "digits of four bytes");
    CHECK(Is("en", "Europe/Kaliningrad", 606870000, mloc_zoneStyleGenericLong,
             "Eastern European Time (Kaliningrad)"),
          "a region's preferred zone, the last of its metazone's");
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
    CHECK(mlocZoneFormatter_Format(&formatter, &zone, &instant, &output) == mloc_success &&
              s_partCount == 1 && s_partKind == mloc_datePartTimeZone && s_partStart == 0 &&
              s_partLength == 12,
          "one part, the zone");
    mlocOutput small = {.buffer = text, .capacity = 4};
    CHECK(mlocZoneFormatter_Format(&formatter, &zone, &instant, &small) == mloc_errorCapacity &&
              small.length == 12,
          "too small a buffer: the size needed");
    mlocOutput measure = {0};
    CHECK(mlocZoneFormatter_Format(&formatter, &zone, &instant, &measure) == mloc_errorCapacity &&
              measure.length == 12,
          "measuring");
}

// Whether an instant's local date, time or date-time in a zone is want
// in a locale, by date options (a zone's in them for a date alone), time
// options, or both.
static bool InZone(const char* id, const char* zoneId, int64_t seconds,
                   const mlocDateTimeOptions* o, char kind, const char* want)
{
    mlocLocale locale;
    mlocTimeZone zone = {0};
    char text[256];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocInstant instant = {.seconds = seconds};
    mlocDateFormatter date;
    mlocTimeFormatter time;
    mlocDateTimeFormatter both;
    mlocResult result =
        mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status == mloc_success
            ? mlocFindTimeZone(&s_data, zoneId, strlen(zoneId), &zone)
            : mloc_errorSyntax;
    if (result >= 0 && kind == 'D')
    {
        result = mlocInitDateFormatter(&s_data, &locale, &o->date, &date);
        result =
            result < 0 ? result : mlocDateFormatter_FormatInZone(&date, &zone, &instant, &output);
    }
    else if (result >= 0 && kind == 'T')
    {
        result = mlocInitTimeFormatter(&s_data, &locale, &o->time, &time);
        result =
            result < 0 ? result : mlocTimeFormatter_FormatInZone(&time, &zone, &instant, &output);
    }
    else if (result >= 0)
    {
        result = mlocInitDateTimeFormatter(&s_data, &locale, o, &both);
        result = result < 0 ? result
                            : mlocDateTimeFormatter_FormatInZone(&both, &zone, &instant, &output);
    }
    bool same = result == mloc_success && output.length == strlen(want) &&
                memcmp(text, want, output.length) == 0;
    if (!same)
    {
        printf("  %s %s: %d \"%.*s\", want \"%s\"\n", id, zoneId, result, (int)output.length, text,
               want);
    }
    return same;
}

#define NNBSP "\xE2\x80\xAF"

static void TestInZone(void)
{
    mlocDateTimeOptions full = {.time = {.style = mloc_timeStyleFull}};
    mlocDateTimeOptions longTime = {.time = {.style = mloc_timeStyleLong}};
    CHECK(InZone("en", "America/Los_Angeles", july, &full, 'T',
                 "8:33:20" NNBSP "PM Pacific Daylight Time") &&
              InZone("en", "America/Los_Angeles", july, &longTime, 'T', "8:33:20" NNBSP "PM PDT") &&
              InZone("ja", "Asia/Tokyo", january, &full, 'T',
                     "8\xE6\x99\x82"
                     "06\xE5\x88\x86"
                     "40\xE7\xA7\x92 "
                     "\xE6\x97\xA5\xE6\x9C\xAC\xE6\xA8\x99\xE6\xBA\x96\xE6\x99\x82"),
          "the full and long time formats, which name the zone");
    mlocDateTimeOptions styles = {.date = {.style = mloc_dateStyleFull},
                                  .time = {.style = mloc_timeStyleFull}};
    CHECK(InZone("en", "America/Los_Angeles", july, &styles, 'B',
                 "Monday, July 13, 2026 at 8:33:20" NNBSP "PM Pacific Daylight Time"),
          "a full date and time");
    mlocDateTimeOptions minute = {
        .date = {.length = mloc_dateLengthMedium},
        .time = {.precision = mloc_timePrecisionMinute, .zone = mloc_zoneStyleSpecificShort}};
    mlocDateTimeOptions weekday = {
        .date = {.fields = mloc_dateFieldSetWeekday, .length = mloc_dateLengthMedium},
        .time = {.precision = mloc_timePrecisionMinute, .zone = mloc_zoneStyleGenericShort}};
    mlocDateTimeOptions berlin = {
        .date = {.fields = mloc_dateFieldSetYearMonthDayWeekday},
        .time = {.precision = mloc_timePrecisionMinute, .zone = mloc_zoneStyleSpecificLong}};
    CHECK(InZone("en", "America/Los_Angeles", july, &minute, 'B',
                 "Jul 13, 2026, 8:33" NNBSP "PM PDT") &&
              InZone("en", "America/Los_Angeles", july, &weekday, 'B', "Mon, 8:33" NNBSP "PM PT") &&
              InZone("de", "Europe/Berlin", january, &berlin, 'B',
                     "Samstag, 10. Januar 2026 um 00:06 Mitteleurop\xC3\xA4ische Normalzeit"),
          "field sets with a zone: the glue, a lone weekday, the local date");
    mlocDateTimeOptions hour = {.date = {.length = mloc_dateLengthMedium},
                                .time = {.precision = mloc_timePrecisionMinuteOptional,
                                         .zone = mloc_zoneStyleSpecificShort}};
    mlocDateTimeOptions offset = {.time = {.hourCycle = mloc_hourCycleClock12,
                                           .precision = mloc_timePrecisionMinute,
                                           .zone = mloc_zoneStyleOffsetShort}};
    CHECK(InZone("en", "America/Los_Angeles", july - 2000, &hour, 'B',
                 "Jul 13, 2026, 8" NNBSP "PM PDT") &&
              InZone("en", "America/Los_Angeles", july, &hour, 'B',
                     "Jul 13, 2026, 8:33" NNBSP "PM PDT") &&
              InZone("en", "America/Los_Angeles", july, &offset, 'T', "8:33" NNBSP "PM GMT-7"),
          "minutes unless zero, with a zone; the short offset");
    mlocDateTimeOptions location = {.time = {.hourCycle = mloc_hourCycleClock12,
                                             .precision = mloc_timePrecisionMinute,
                                             .zone = mloc_zoneStyleLocation}};
    mlocDateTimeOptions city = location;
    city.time.zone = mloc_zoneStyleCity;
    CHECK(InZone("en", "America/Los_Angeles", july, &location, 'T',
                 "8:33" NNBSP "PM Los Angeles Time") &&
              InZone("en", "America/Los_Angeles", july, &city, 'T', "8:33" NNBSP "PM Los Angeles"),
          "a time with the location, with the city");
    CHECK(InZone("en-u-hc-h23", "America/Los_Angeles", july, &full, 'T',
                 "20:33:20 Pacific Daylight Time") &&
              InZone("en-u-hc-h23", "America/Los_Angeles", july, &longTime, 'T', "20:33:20 PDT"),
          "the full and long formats under an hour cycle asked for");
    mlocDateTimeOptions generic = {
        .date = {.length = mloc_dateLengthMedium, .zone = mloc_zoneStyleGenericShort}};
    mlocDateTimeOptions sofia = {
        .date = {.length = mloc_dateLengthShort, .zone = mloc_zoneStyleSpecificShort}};
    CHECK(InZone("en", "America/Los_Angeles", july, &generic, 'D', "Jul 13, 2026, PT") &&
              InZone("bg", "Europe/Sofia", january, &sofia, 'D',
                     "10.01.26" NNBSP "\xD0\xB3., \xD0\x93\xD1\x80\xD0\xB8\xD0\xBD\xD1\x83"
                     "\xD0\xB8\xD1\x87+2 \xD1\x87."),
          "a date with a zone, the zone's pattern's literals kept");
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
        mlocInitZoneFormatter(&s_data, &en, mloc_zoneStyleNone, &formatter) == mloc_errorInvalid &&
            mlocInitZoneFormatter(&s_data, &en, (mlocZoneStyle)9, &formatter) ==
                mloc_errorInvalid &&
            mlocInitZoneFormatter(&s_data, &en, mloc_zoneStyleCity, nullptr) == mloc_errorInvalid &&
            mlocInitZoneFormatter(nullptr, &en, mloc_zoneStyleCity, &formatter) ==
                mloc_errorInvalid &&
            mlocInitZoneFormatter(&s_data, nullptr, mloc_zoneStyleCity, &formatter) ==
                mloc_errorInvalid,
        "no style, a style out of range, null pointers");
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocInitZoneFormatter(&builtin, &en, mloc_zoneStyleCity, &formatter) ==
              mloc_errorNotFound,
          "data without the zone names");
    CHECK(mlocInitZoneFormatter(&s_data, &en, mloc_zoneStyleCity, &formatter) == mloc_success,
          "a formatter");
    mlocInstant far = {.seconds = INT64_C(8640000000001)};
    mlocInstant fraction = {.seconds = 0, .nanoseconds = 1000000000};
    mlocTimeZone forged = zone;
    forged.offsetMinutes = 30;
    CHECK(
        mlocZoneFormatter_Format(nullptr, &zone, &instant, &output) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&formatter, nullptr, &instant, &output) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&formatter, &zone, nullptr, &output) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&formatter, &zone, &instant, nullptr) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&formatter, &zone, &far, &output) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&formatter, &zone, &fraction, &output) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&formatter, &forged, &instant, &output) == mloc_errorInvalid,
        "null pointers, an instant out of range, a zone the library did not make");
    mlocOutput noBuffer = {.capacity = 4};
    CHECK(mlocZoneFormatter_Format(&formatter, &zone, &instant, &noBuffer) == mloc_errorInvalid,
          "a capacity without a buffer");
    mlocZoneFormatter spoiled = formatter;
    memset(&spoiled, 0, sizeof(spoiled));
    CHECK(mlocZoneFormatter_Format(&spoiled, &zone, &instant, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
}

// The zoned formatters refuse a zone the library did not make, alone and
// at either end of a range, before reading the zone at the instant.
static void TestForgedInZone(void)
{
    mlocLocale en;
    mlocTimeZone zone = {0};
    mlocDateFormatter date;
    mlocTimeFormatter time;
    mlocDateTimeFormatter both;
    mlocDateOptions dateOptions = {.zone = mloc_zoneStyleSpecificShort};
    mlocTimeOptions timeOptions = {.zone = mloc_zoneStyleSpecificShort};
    mlocDateTimeOptions bothOptions = {.time = timeOptions};
    bool made = mlocParseLocale("en", 2, &en).status == mloc_success &&
                mlocFindTimeZone(&s_data, "America/New_York", 16, &zone) == mloc_success &&
                mlocInitDateFormatter(&s_data, &en, &dateOptions, &date) >= 0 &&
                mlocInitTimeFormatter(&s_data, &en, &timeOptions, &time) >= 0 &&
                mlocInitDateTimeFormatter(&s_data, &en, &bothOptions, &both) >= 0;
    mlocTimeZone forged = zone;
    forged.offsetMinutes = 30;
    mlocInstant at = {.seconds = 1792263845};
    mlocInstant later = {.seconds = 1792263845 + 90000};
    char text[128];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    CHECK(made && mlocDateFormatter_FormatInZone(&date, &zone, &at, &output) == mloc_success &&
              mlocDateFormatter_FormatInZone(&date, &forged, &at, &output) == mloc_errorInvalid &&
              mlocTimeFormatter_FormatInZone(&time, &forged, &at, &output) == mloc_errorInvalid &&
              mlocDateTimeFormatter_FormatInZone(&both, &forged, &at, &output) ==
                  mloc_errorInvalid &&
              mlocDateFormatter_FormatRangeInZone(&date, &forged, &at, &later, &output) ==
                  mloc_errorInvalid &&
              mlocTimeFormatter_FormatRangeInZone(&time, &forged, &at, &later, &output) ==
                  mloc_errorInvalid &&
              mlocDateTimeFormatter_FormatRangeInZone(&both, &forged, &at, &later, &output) ==
                  mloc_errorInvalid,
          "a zone the library did not make, to each zoned formatter");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 4};
    if (!OpenFile(MLOC_ZONE_NAMES_FILE, &s_sources[1]) ||
        !OpenFile(MLOC_LOCALE_NAMES_FILE, &s_sources[2]) ||
        !OpenFile(MLOC_DATES_FILE, &s_sources[3]))
    {
        fputs("test_zone_format: cannot open the data files\n", stderr);
        return 2;
    }
    TestStyles();
    TestFallbacks();
    TestGeneric();
    TestOffsets();
    TestEdges();
    TestOutput();
    TestInZone();
    TestRefused();
    TestForgedInZone();
    return s_failures == 0 ? 0 : 1;
}
