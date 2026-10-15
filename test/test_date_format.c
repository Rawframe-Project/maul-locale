// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date formatter (maul-locale/datetime.h) on the compiled-in data,
// dates.mloc and spellout.mloc: each field set, length and year style,
// the standard formats, the locale's calendar and digits, the systems a
// pattern names for its fields, the Hebrew and Chinese months, parts,
// measuring, and the arguments refused.

#define _CRT_SECURE_NO_WARNINGS

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

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

static mlocDate Iso(int32_t year, uint32_t month, uint32_t day)
{
    mlocDate date = {0};
    mlocDateInput input = {.year = year, .month = month, .day = day};
    if (mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &date) != mloc_success)
    {
        date.day = 0;
    }
    return date;
}

// Whether a locale formats a date as want with the options.
static bool Is(const char* id, mlocDateOptions options, mlocDate date, const char* want)
{
    mlocLocale locale;
    mlocDateFormatter formatter;
    char text[256];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status != mloc_success ||
        mlocInitDateFormatter(&formatter, &s_data, &locale, &options) < 0 ||
        mlocFormatDate(&formatter, &date, &output) != mloc_success)
    {
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0;
    if (!same)
    {
        printf("  %s: got \"%.*s\", want \"%s\"\n", id, (int)output.length, text, want);
    }
    return same;
}

static mlocDateOptions Set(mlocDateFieldSet fields, mlocDateLength length)
{
    return (mlocDateOptions){.fields = fields, .length = length};
}

static void TestFieldSets(void)
{
    mlocDate date = Iso(2026, 10, 15);
    CHECK(Is("en", Set(mloc_dateFieldSetDay, mloc_dateLengthLong), date, "15"), "a day");
    CHECK(Is("en", Set(mloc_dateFieldSetWeekday, mloc_dateLengthLong), date, "Thursday"),
          "a weekday");
    CHECK(Is("en", Set(mloc_dateFieldSetWeekday, mloc_dateLengthShort), date, "T"),
          "a lone weekday, short");
    CHECK(Is("en", Set(mloc_dateFieldSetDayWeekday, mloc_dateLengthMedium), date, "15 Thu"),
          "a day and weekday");
    CHECK(Is("en", Set(mloc_dateFieldSetMonthDay, mloc_dateLengthMedium), date, "Oct 15"),
          "a month and day");
    CHECK(Is("en", Set(mloc_dateFieldSetMonthDayWeekday, mloc_dateLengthLong), date,
             "Thursday, October 15"),
          "a month, day and weekday");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort), date, "10/15/26"),
          "a year, month and day, short");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonthDayWeekday, mloc_dateLengthLong), date,
             "Thursday, October 15, 2026"),
          "everything, long");
    CHECK(Is("en", Set(mloc_dateFieldSetMonth, mloc_dateLengthLong), date, "October"),
          "a month alone");
    CHECK(Is("en", Set(mloc_dateFieldSetYear, mloc_dateLengthMedium), date, "2026"), "a year");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonth, mloc_dateLengthMedium), date, "Oct 2026"),
          "a year and month");
    CHECK(Is("en", (mlocDateOptions){0}, date, "October 15, 2026"), "the default options");
}

static void TestYearStyles(void)
{
    mlocDateOptions shortDate = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort);
    CHECK(Is("en", shortDate, Iso(2049, 1, 2), "1/2/49") &&
              Is("en", shortDate, Iso(1950, 1, 2), "1/2/50"),
          "a year from 1950 to 2049 in two digits");
    CHECK(Is("en", shortDate, Iso(2050, 1, 2), "1/2/2050") &&
              Is("en", shortDate, Iso(1949, 1, 2), "1/2/1949"),
          "its century outside them");
    CHECK(Is("en", shortDate, Iso(999, 1, 2), "1/2/999 AD") &&
              Is("en", shortDate, Iso(-49, 1, 2), "1/2/50 BC"),
          "its era before the year 1000 or the common era");
    shortDate.yearStyle = mloc_yearStyleFull;
    CHECK(Is("en", shortDate, Iso(2026, 1, 2), "1/2/2026") &&
              Is("en", shortDate, Iso(-49, 1, 2), "1/2/50 BC"),
          "full: the century, the era when needed");
    shortDate.yearStyle = mloc_yearStyleWithEra;
    CHECK(Is("en", shortDate, Iso(2026, 1, 2), "1/2/2026 AD"), "with its era");
    CHECK(Is("th-TH", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort), Iso(2000, 1, 1),
             "1/1/43"),
          "a Buddhist year counted from the Gregorian, in two digits");
}

static void TestStyles(void)
{
    mlocDate date = Iso(2026, 10, 15);
    CHECK(Is("en", (mlocDateOptions){.style = mloc_dateStyleFull}, date,
             "Thursday, October 15, 2026") &&
              Is("en", (mlocDateOptions){.style = mloc_dateStyleShort}, date, "10/15/26"),
          "the standard formats");
    CHECK(Is("de", (mlocDateOptions){.style = mloc_dateStyleMedium}, date, "15.10.2026"),
          "German's medium date");
}

static void TestCalendars(void)
{
    mlocDateOptions longDate = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthLong);
    CHECK(Is("th", longDate, Iso(2026, 10, 15), "15 ตุลาคม 2569"), "Thailand's Buddhist calendar");
    CHECK(Is("en-u-ca-japanese", longDate, Iso(2019, 5, 1), "May 1, 1 Reiwa"),
          "a Japanese era's first year");
    CHECK(Is("ja-u-ca-japanese", longDate, Iso(2019, 5, 1), "令和元年5月1日"),
          "its year written by jpanyear");
    CHECK(Is("ja-u-ca-japanese", longDate, Iso(1868, 1, 1), "西暦1868年1月1日"),
          "a date before Meiji by the Gregorian era");
    CHECK(Is("en-u-ca-hebrew", longDate, Iso(2024, 2, 20), "11 Adar I 5784") &&
              Is("en-u-ca-hebrew", longDate, Iso(2024, 3, 15), "5 Adar II 5784") &&
              Is("en-u-ca-hebrew", longDate, Iso(2025, 3, 15), "15 Adar 5785"),
          "Adar I and Adar II in a leap year, Adar in another");
    CHECK(Is("he-u-ca-hebrew", (mlocDateOptions){.style = mloc_dateStyleLong}, Iso(2026, 10, 15),
             "ד׳ בחשוון תשפ״ז"),
          "a Hebrew date in Hebrew letters, the thousands left out");
    CHECK(Is("zh-u-ca-chinese", (mlocDateOptions){.style = mloc_dateStyleLong}, Iso(2023, 3, 22),
             "2023癸卯年闰二月初一"),
          "a Chinese leap month and cyclic year");
    CHECK(Is("en-u-ca-chinese", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort),
             Iso(2023, 3, 22), "2bis/1/2023"),
          "a numeric leap month");
    CHECK(Is("en-u-ca-iso8601", longDate, Iso(2026, 10, 15), "October 15, 2026"),
          "the ISO calendar by the Gregorian data");
    CHECK(Is("en-u-ca-persian", longDate, Iso(500, 6, 15), "Khordad 25, -121 AP"),
          "a Persian year before its epoch");
}

static void TestDigits(void)
{
    mlocDateOptions shortDate = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort);
    CHECK(Is("fa", shortDate, Iso(2026, 10, 15), "۱۴۰۵/۷/۲۳"), "the locale's own digits");
    CHECK(Is("en-u-nu-arab", shortDate, Iso(2026, 10, 15), "١٠/١٥/٢٦"), "nu's digits");
    CHECK(Is("zh-u-ca-chinese", (mlocDateOptions){.style = mloc_dateStyleLong}, Iso(2026, 10, 15),
             "2026丙午年九月初六"),
          "a day written by hanidays");
}

static void ReportPart(void* context, uint32_t part, size_t start, size_t length)
{
    char* parts = context;
    char item[32];
    snprintf(item, sizeof(item), "%u:%zu:%zu ", part, start, length);
    strcat(parts, item);
}

static void TestOutput(void)
{
    mlocLocale en;
    mlocDateFormatter formatter;
    mlocDate date = Iso(2026, 10, 15);
    mlocDateOptions options = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitDateFormatter(&formatter, &s_data, &en, &options) == mloc_success,
          "a formatter");
    char parts[256] = "";
    char text[64];
    mlocOutput output = {
        .buffer = text, .capacity = sizeof(text), .part = ReportPart, .context = parts};
    CHECK(mlocFormatDate(&formatter, &date, &output) == mloc_success &&
              strcmp(parts, "5:0:3 0:3:1 7:4:2 0:6:2 2:8:4 ") == 0,
          "the parts of \"Oct 15, 2026\"");
    mlocOutput measure = {0};
    CHECK(mlocFormatDate(&formatter, &date, &measure) == mloc_errorCapacity && measure.length == 12,
          "measuring");
    mlocOutput small = {.buffer = text, .capacity = 5};
    CHECK(mlocFormatDate(&formatter, &date, &small) == mloc_errorCapacity && small.length == 12,
          "a buffer too small");
}

static void TestRefusals(void)
{
    mlocLocale en;
    mlocDateFormatter formatter;
    memset(&formatter, 0x5A, sizeof(formatter));
    mlocDateFormatter before = formatter;
    mlocDate date = Iso(2026, 10, 15);
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success, "a locale");
    CHECK(mlocInitDateFormatter(nullptr, &s_data, &en, nullptr) == mloc_errorInvalid &&
              mlocInitDateFormatter(&formatter, nullptr, &en, nullptr) == mloc_errorInvalid &&
              mlocInitDateFormatter(&formatter, &s_data, nullptr, nullptr) == mloc_errorInvalid,
          "null pointers");
    mlocDateOptions bad[] = {{.fields = (mlocDateFieldSet)10},
                             {.length = (mlocDateLength)3},
                             {.yearStyle = (mlocYearStyle)3},
                             {.style = (mlocDateStyle)5}};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        CHECK(mlocInitDateFormatter(&formatter, &s_data, &en, &bad[i]) == mloc_errorInvalid,
              "options out of range");
    }
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocInitDateFormatter(&formatter, &builtin, &en, nullptr) == mloc_errorNotFound,
          "data without the dates");
    CHECK(memcmp(&formatter, &before, sizeof(formatter)) == 0, "the formatter left unchanged");
    CHECK(mlocFormatDate(&formatter, &date, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
    CHECK(mlocInitDateFormatter(&formatter, &s_data, &en, nullptr) == mloc_success &&
              mlocFormatDate(&formatter, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatDate(&formatter, &date, nullptr) == mloc_errorInvalid,
          "a null date or output");
    mlocOutput noBuffer = {.capacity = 4};
    CHECK(mlocFormatDate(&formatter, &date, &noBuffer) == mloc_errorInvalid,
          "a capacity without a buffer");
    mlocDate far = {.day = mlocDateMaxDay + 1};
    CHECK(mlocFormatDate(&formatter, &far, &output) == mloc_errorInvalid,
          "a date outside Temporal's range");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    if (!OpenFile(MLOC_DATES_FILE, &s_sources[1]) || !OpenFile(MLOC_SPELLOUT_FILE, &s_sources[2]))
    {
        fputs("test_date_format: cannot open " MLOC_DATES_FILE " or " MLOC_SPELLOUT_FILE "\n",
              stderr);
        return 2;
    }
    s_data = (mlocDataSet){s_sources, 3};
    TestFieldSets();
    TestYearStyles();
    TestStyles();
    TestCalendars();
    TestDigits();
    TestOutput();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
