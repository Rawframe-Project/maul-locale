// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formats dates and times read one per line from standard input, for
// tools/check_datetime.py to compare with CLDR's datetime conformance data
// and ICU4C. A date's line is tab-separated: the locale (its -u-ca key
// naming the calendar), the field set, length, year style and standard
// style as mlocDateOptions' numbers, and an ISO date (YYYY-MM-DD). A
// time's line is "T", the locale, the hour cycle, precision and standard
// style as mlocTimeOptions' numbers, and the time (HH:MM:SS.nnnnnnnnn). A
// date-time's line is "DT", the locale, the date options' four numbers,
// the time options' three, the glue, the date and the time. A zone's line
// is "Z", the locale, the zone's identifier, the instant's seconds since
// 1970 and the style as mlocZoneStyle's number. A date, time or date-time
// in a zone is "ZD", "ZT" or "ZDT", the locale, the zone's identifier, the
// instant's seconds, and the options' numbers: the date's five (the zone's
// last), the time's four (likewise), or the date's five, the time's four
// and the glue. The output line is the formatted text, or "error N" with
// the library's result.
//
// usage: conform_datetime dates.mloc spellout.mloc [zone-names.mloc
//        names.mloc] < lines

#define _CRT_SECURE_NO_WARNINGS

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocData s_sources[5];

// Reads a data file into memory and opens it as source index.
static bool Open(const char* path, int index)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    uint8_t* bytes = malloc((size_t)16 << 20);
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)16 << 20, file);
    fclose(file);
    return read > 0 && mlocOpenData(bytes, read, &s_sources[index]) == mloc_success;
}

static mlocResult Format(const mlocDataSet* set, const char* id, const mlocDateOptions* options,
                         int year, int month, int day, char* out, size_t* length)
{
    mlocLocale locale;
    mlocDate date;
    mlocDateFormatter formatter;
    mlocDateInput input = {.year = year, .month = (uint32_t)month, .day = (uint32_t)day};
    if (mlocCanonicalizeLocale(set, id, strlen(id), &locale, nullptr).status != mloc_success)
    {
        return mloc_errorSyntax;
    }
    mlocResult result = mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &date);
    result = result < 0 ? result : mlocInitDateFormatter(set, &locale, options, &formatter);
    if (result < 0)
    {
        return result;
    }
    mlocOutput output = {.buffer = out, .capacity = 1024};
    result = mlocDateFormatter_Format(&formatter, &date, &output);
    *length = output.length;
    return result;
}

static mlocResult FormatTime(const mlocDataSet* set, const char* line, char* out, size_t* length)
{
    char id[256];
    int cycle = 0;
    int precision = 0;
    int style = 0;
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    unsigned long nanosecond = 0;
    if (sscanf(line, "T %255s %d %d %d %u:%u:%u.%lu", id, &cycle, &precision, &style, &hour,
               &minute, &second, &nanosecond) != 8)
    {
        return mloc_errorSyntax;
    }
    mlocLocale locale;
    mlocTimeFormatter formatter;
    mlocTimeOptions options = {(mlocHourCycle)cycle, (mlocTimePrecision)precision,
                               (mlocTimeStyle)style, mloc_zoneStyleNone};
    mlocTime time = {.hour = (uint8_t)hour,
                     .minute = (uint8_t)minute,
                     .second = (uint8_t)second,
                     .nanosecond = (uint32_t)nanosecond};
    if (mlocCanonicalizeLocale(set, id, strlen(id), &locale, nullptr).status != mloc_success)
    {
        return mloc_errorSyntax;
    }
    mlocResult result = mlocInitTimeFormatter(set, &locale, &options, &formatter);
    if (result < 0)
    {
        return result;
    }
    mlocOutput output = {.buffer = out, .capacity = 1024};
    result = mlocTimeFormatter_Format(&formatter, &time, &output);
    *length = output.length;
    return result;
}

static mlocResult FormatDateTime(const mlocDataSet* set, const char* line, char* out,
                                 size_t* length)
{
    char id[256];
    int n[8] = {0};
    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    unsigned long nanosecond = 0;
    if (sscanf(line, "DT %255s %d %d %d %d %d %d %d %d %d-%u-%u %u:%u:%u.%lu", id, &n[0], &n[1],
               &n[2], &n[3], &n[4], &n[5], &n[6], &n[7], &year, &month, &day, &hour, &minute,
               &second, &nanosecond) != 16)
    {
        return mloc_errorSyntax;
    }
    mlocDateTimeOptions options = {
        {(mlocDateFieldSet)n[0], (mlocDateLength)n[1], (mlocYearStyle)n[2], (mlocDateStyle)n[3],
         mloc_zoneStyleNone},
        {(mlocHourCycle)n[4], (mlocTimePrecision)n[5], (mlocTimeStyle)n[6], mloc_zoneStyleNone},
        (mlocDateTimeGlue)n[7]};
    mlocDateTime value = {.time = {.hour = (uint8_t)hour,
                                   .minute = (uint8_t)minute,
                                   .second = (uint8_t)second,
                                   .nanosecond = (uint32_t)nanosecond}};
    mlocDateInput input = {.year = year, .month = month, .day = day};
    mlocLocale locale;
    mlocDateTimeFormatter formatter;
    if (mlocCanonicalizeLocale(set, id, strlen(id), &locale, nullptr).status != mloc_success)
    {
        return mloc_errorSyntax;
    }
    mlocResult result =
        mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &value.date);
    result = result < 0 ? result : mlocInitDateTimeFormatter(set, &locale, &options, &formatter);
    if (result < 0)
    {
        return result;
    }
    mlocOutput output = {.buffer = out, .capacity = 1024};
    result = mlocDateTimeFormatter_Format(&formatter, &value, &output);
    *length = output.length;
    return result;
}

static mlocResult FormatZone(const mlocDataSet* set, const char* line, char* out, size_t* length)
{
    char id[256];
    char zoneId[256];
    long long seconds = 0;
    int style = 0;
    if (sscanf(line, "Z %255s %255s %lld %d", id, zoneId, &seconds, &style) != 4)
    {
        return mloc_errorSyntax;
    }
    // The formatters of the last locale, by style, kept for the lines that
    // follow (a sweep's lines come grouped by locale).
    static char s_id[256];
    static mlocLocale s_locale;
    static mlocZoneFormatter s_formatters[mloc_zoneStyleOffsetShort + 1];
    static mlocResult s_results[mloc_zoneStyleOffsetShort + 1];
    static bool s_made[mloc_zoneStyleOffsetShort + 1];
    if (style < 0 || style > mloc_zoneStyleOffsetShort)
    {
        return mloc_errorInvalid;
    }
    if (strcmp(id, s_id) != 0)
    {
        memset(s_made, 0, sizeof(s_made));
        snprintf(s_id, sizeof(s_id), "%s", id);
        if (mlocCanonicalizeLocale(set, id, strlen(id), &s_locale, nullptr).status != mloc_success)
        {
            s_id[0] = '\0';
            return mloc_errorSyntax;
        }
    }
    if (!s_made[style])
    {
        s_results[style] =
            mlocInitZoneFormatter(set, &s_locale, (mlocZoneStyle)style, &s_formatters[style]);
        s_made[style] = true;
    }
    mlocTimeZone zone;
    mlocResult result = mlocFindTimeZone(set, zoneId, strlen(zoneId), &zone);
    result = result < 0 ? result : s_results[style];
    if (result < 0)
    {
        return result;
    }
    mlocInstant instant = {.seconds = seconds};
    mlocOutput output = {.buffer = out, .capacity = 1024};
    result = mlocZoneFormatter_Format(&s_formatters[style], &zone, &instant, &output);
    *length = output.length;
    return result;
}

// The options of a "ZD", "ZT" or "ZDT" line after its instant.
static bool ReadInZone(const char* rest, int kind, mlocDateTimeOptions* o)
{
    int n[10] = {0};
    int want = kind == 'D' ? 5 : kind == 'T' ? 4 : 10;
    int read = kind == 'T' ? sscanf(rest, "%d %d %d %d", &n[5], &n[6], &n[7], &n[8])
                           : sscanf(rest, "%d %d %d %d %d %d %d %d %d %d", &n[0], &n[1], &n[2],
                                    &n[3], &n[4], &n[5], &n[6], &n[7], &n[8], &n[9]);
    *o = (mlocDateTimeOptions){
        {(mlocDateFieldSet)n[0], (mlocDateLength)n[1], (mlocYearStyle)n[2], (mlocDateStyle)n[3],
         (mlocZoneStyle)n[4]},
        {(mlocHourCycle)n[5], (mlocTimePrecision)n[6], (mlocTimeStyle)n[7], (mlocZoneStyle)n[8]},
        (mlocDateTimeGlue)n[9]};
    return read == want;
}

static mlocResult FormatInZone(const mlocDataSet* set, const char* line, char* out, size_t* length)
{
    char kind[4];
    char id[256];
    char zoneId[256];
    long long seconds = 0;
    int used = 0;
    mlocDateTimeOptions o;
    mlocLocale locale;
    mlocTimeZone zone;
    if (sscanf(line, "%3s %255s %255s %lld%n", kind, id, zoneId, &seconds, &used) != 4 ||
        !ReadInZone(line + used, kind[2] == 'T' ? 'B' : kind[1], &o) ||
        mlocCanonicalizeLocale(set, id, strlen(id), &locale, nullptr).status != mloc_success)
    {
        return mloc_errorSyntax;
    }
    mlocInstant instant = {.seconds = seconds};
    mlocOutput output = {.buffer = out, .capacity = 1024};
    mlocResult result = mlocFindTimeZone(set, zoneId, strlen(zoneId), &zone);
    if (result >= 0 && strcmp(kind, "ZD") == 0)
    {
        mlocDateFormatter f;
        result = mlocInitDateFormatter(set, &locale, &o.date, &f);
        result = result < 0 ? result : mlocDateFormatter_FormatInZone(&f, &zone, &instant, &output);
    }
    else if (result >= 0 && strcmp(kind, "ZT") == 0)
    {
        mlocTimeFormatter f;
        result = mlocInitTimeFormatter(set, &locale, &o.time, &f);
        result = result < 0 ? result : mlocTimeFormatter_FormatInZone(&f, &zone, &instant, &output);
    }
    else if (result >= 0)
    {
        mlocDateTimeFormatter f;
        result = mlocInitDateTimeFormatter(set, &locale, &o, &f);
        result =
            result < 0 ? result : mlocDateTimeFormatter_FormatInZone(&f, &zone, &instant, &output);
    }
    *length = output.length;
    return result;
}

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 5)
    {
        fputs("usage: conform_datetime dates.mloc spellout.mloc [zone-names.mloc names.mloc] < "
              "lines\n",
              stderr);
        return 2;
    }
    mlocOpenBuiltinData(&s_sources[0]);
    for (int i = 1; i < argc; i++)
    {
        if (!Open(argv[i], i))
        {
            fputs("conform_datetime: cannot open the data files\n", stderr);
            return 2;
        }
    }
    mlocDataSet set = {s_sources, (size_t)argc};
    char line[1024];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        char id[256];
        int fields = 0;
        int length = 0;
        int yearStyle = 0;
        int style = 0;
        int year = 0;
        int month = 0;
        int day = 0;
        char out[1024];
        size_t written = 0;
        if (line[0] == 'T' || line[0] == 'Z' || (line[0] == 'D' && line[1] == 'T'))
        {
            bool inZone = line[0] == 'Z' && (line[1] == 'D' || line[1] == 'T');
            mlocResult result = inZone           ? FormatInZone(&set, line, out, &written)
                                : line[0] == 'T' ? FormatTime(&set, line, out, &written)
                                : line[0] == 'Z' ? FormatZone(&set, line, out, &written)
                                                 : FormatDateTime(&set, line, out, &written);
            if (result < 0)
            {
                printf("error %d\n", result);
                continue;
            }
            printf("%.*s\n", (int)written, out);
            continue;
        }
        if (sscanf(line, "%255s %d %d %d %d %d-%d-%d", id, &fields, &length, &yearStyle, &style,
                   &year, &month, &day) != 8)
        {
            puts("error syntax");
            continue;
        }
        mlocDateOptions options = {(mlocDateFieldSet)fields, (mlocDateLength)length,
                                   (mlocYearStyle)yearStyle, (mlocDateStyle)style,
                                   mloc_zoneStyleNone};
        mlocResult result = Format(&set, id, &options, year, month, day, out, &written);
        if (result < 0)
        {
            printf("error %d\n", result);
            continue;
        }
        printf("%.*s\n", (int)written, out);
    }
    return 0;
}
