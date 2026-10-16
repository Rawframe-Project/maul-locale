// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date ranges (mlocFormatDateRange, research 40): the greatest
// difference's pair, the fallback, one date, the order and the parts of
// each date, checked against ICU4C 79's DateIntervalFormat made from
// each option's skeleton; where the library differs on purpose (a leap
// month, fa's month items, a calendar ICU lacks), its own answer. Then
// ICU's answers kept in test/date_ranges_icu.tsv, with the library's own
// where it differs and why.

// fopen is the portable way to read the data file; MSVC's CRT deprecates
// it.
#define _CRT_SECURE_NO_WARNINGS

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The range's dash between thin spaces, as en, de and kek write it.
#define DASH "\xE2\x80\x89\xE2\x80\x93\xE2\x80\x89"

static mlocData s_sources[4];
static mlocDataSet s_data;

static bool OpenFile(const char* path, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    // Each file's own size, which the web build's memory holds.
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    uint8_t* bytes = size > 0 && fseek(file, 0, SEEK_SET) == 0 ? malloc((size_t)size) : nullptr;
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)size, file);
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

// The parts written, each as its kind's letter (L literal, G era, y year,
// M month, E weekday, d day), its date ('=' both, '<' the start, '>' the
// end) and its text in brackets.
typedef struct Parts
{
    const char* text;
    char out[512];
    size_t length;
} Parts;

static void Part(void* context, uint32_t part, size_t start, size_t length)
{
    static const char s_kinds[] = "LGyrUMEd";
    Parts* p = context;
    uint32_t kind = part & 0xFF;
    char side = (part & 0x300) == mloc_dateRangeStart ? '<'
                : (part & 0x300) == mloc_dateRangeEnd ? '>'
                                                      : '=';
    int n = snprintf(p->out + p->length, sizeof(p->out) - p->length, "%c%c[%.*s]",
                     kind < sizeof(s_kinds) - 1 ? s_kinds[kind] : '?', side, (int)length,
                     p->text + start);
    p->length += n > 0 ? (size_t)n : 0;
}

static bool Formatter(const char* id, mlocDateOptions options, mlocDateFormatter* formatter)
{
    mlocLocale locale;
    return mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status ==
               mloc_success &&
           mlocInitDateFormatter(formatter, &s_data, &locale, &options) >= 0;
}

// Whether a locale writes the range as want, and its parts as parts
// when given.
static bool Is(const char* id, mlocDateOptions options, mlocDate start, mlocDate end,
               const char* want, const char* parts)
{
    mlocDateFormatter formatter;
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    if (!Formatter(id, options, &formatter) ||
        mlocFormatDateRange(&formatter, &start, &end, &output) != mloc_success)
    {
        printf("  %s: not formatted\n", id);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s: got \"%.*s\" %s, want \"%s\" %s\n", id, (int)output.length, text, p.out, want,
               parts != nullptr ? parts : "");
    }
    return same;
}

static mlocDateOptions Set(mlocDateFieldSet fields, mlocDateLength length)
{
    return (mlocDateOptions){.fields = fields, .length = length};
}

static void TestDifferences(void)
{
    mlocDateOptions medium = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    CHECK(Is("en", medium, Iso(2026, 1, 5), Iso(2026, 1, 8), "Jan 5" DASH "8, 2026",
             "M=[Jan]L=[ ]d<[5]L=[" DASH "]d>[8]L=[, ]y=[2026]"),
          "a day's pair: the month and year once, each day its date's");
    CHECK(Is("en", medium, Iso(2026, 1, 5), Iso(2026, 2, 8), "Jan 5" DASH "Feb 8, 2026",
             "M<[Jan]L<[ ]d<[5]L=[" DASH "]M>[Feb]L>[ ]d>[8]L=[, ]y=[2026]"),
          "a month's pair, the text between a date's fields its own");
    CHECK(Is("en", medium, Iso(2025, 1, 5), Iso(2026, 2, 8), "Jan 5, 2025" DASH "Feb 8, 2026",
             nullptr),
          "a year's pair");
    CHECK(
        Is("en", medium, Iso(1, 1, 1), Iso(0, 12, 29), "Jan 1, 1 AD" DASH "Dec 29, 1 BC", nullptr),
        "an era's, both in the era style the earlier year needs");
    CHECK(Is("ja-u-ca-japanese", medium, Iso(2019, 4, 28), Iso(2019, 5, 3),
             "\xE5\xB9\xB3\xE6\x88\x90"
             "31\xE5\xB9\xB4"
             "4\xE6\x9C\x88"
             "28\xE6\x97\xA5"
             "\xEF\xBD\x9E\xE4\xBB\xA4\xE5\x92\x8C\xE5\x85\x83\xE5\xB9\xB4"
             "5\xE6\x9C\x88"
             "3\xE6\x97\xA5",
             nullptr),
          "a Japanese era's");
    CHECK(Is("en", medium, Iso(2026, 1, 8), Iso(2026, 1, 5), "Jan 8" DASH "5, 2026", nullptr),
          "a start past its end, as given");
    CHECK(Is("de", medium, Iso(2026, 1, 5), Iso(2026, 2, 8), "05.01." DASH "08.02.2026", nullptr),
          "de's numeric month");
    CHECK(Is("en", (mlocDateOptions){.style = mloc_dateStyleFull}, Iso(2026, 1, 5), Iso(2026, 1, 8),
             "Monday, January 5" DASH "Thursday, January 8, 2026", nullptr),
          "a standard format's skeleton");
}

static void TestOneDateAndFallback(void)
{
    mlocDateOptions medium = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    CHECK(Is("en", medium, Iso(2026, 1, 5), Iso(2026, 1, 5), "Jan 5, 2026",
             "M=[Jan]L=[ ]d=[5]L=[, ]y=[2026]"),
          "equal dates: one, every part both's");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonth, mloc_dateLengthMedium), Iso(2026, 1, 5),
             Iso(2026, 1, 8), "Jan 2026", nullptr),
          "a day's difference a month does not show: one date");
    CHECK(Is("en", Set(mloc_dateFieldSetYear, mloc_dateLengthMedium), Iso(2026, 1, 5),
             Iso(2026, 3, 8), "2026", nullptr),
          "a month's difference a year does not show");
    CHECK(Is("en", Set(mloc_dateFieldSetWeekday, mloc_dateLengthMedium), Iso(2026, 1, 5),
             Iso(2026, 1, 12), "Mon" DASH "Mon", "E<[Mon]L=[" DASH "]E>[Mon]"),
          "a weekday a week apart: its pair, as ICU writes it");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort), Iso(2026, 1, 5),
             Iso(2026, 1, 8), "1/5/26" DASH "1/8/26",
             "M<[1]L<[/]d<[5]L<[/]y<[26]L=[" DASH "]M>[1]L>[/]d>[8]L>[/]y>[26]"),
          "the short date's pair, each whole");
    CHECK(Is("kek", medium, Iso(2026, 1, 5), Iso(2027, 2, 8),
             "8 Xkab\xCA\xBC Po, 2027" DASH "5 Xb\xCA\xBC"
             "een Po, 2026",
             nullptr),
          "kek's fallback, \"{1} \xE2\x80\x93 {0}\": the end first");
}

// Where the library writes the range ICU4C 79 does not.
static void TestOwnAnswers(void)
{
    // ICU compares the month number alone and writes one date.
    CHECK(Is("zh-u-ca-chinese", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium),
             Iso(2025, 6, 29), Iso(2025, 7, 29),
             "2025\xE5\xB9\xB4\xE5\x85\xAD\xE6\x9C\x88\xE5\x88\x9D\xE4\xBA\x94\xE2\x80\x93"
             "2025\xE5\xB9\xB4\xE9\x97\xB0\xE5\x85\xAD\xE6\x9C\x88\xE5\x88\x9D\xE4\xBA\x94",
             nullptr),
          "a leap month differs from the month it follows");
    // fa's "LLLL تا MMMM y G": ICU writes the first month twice.
    mlocDateOptions era = Set(mloc_dateFieldSetYearMonth, mloc_dateLengthLong);
    era.yearStyle = mloc_yearStyleWithEra;
    CHECK(Is("fa-u-ca-gregory", era, Iso(2026, 1, 5), Iso(2026, 2, 8),
             "\xDA\x98\xD8\xA7\xD9\x86\xD9\x88\xDB\x8C\xD9\x87 \xD8\xAA\xD8\xA7 "
             "\xD9\x81\xD9\x88\xD8\xB1\xDB\x8C\xD9\x87 \xDB\xB2\xDB\xB0\xDB\xB2\xDB\xB6 "
             "\xD9\x85.",
             nullptr),
          "a month met twice as LLLL and MMMM is split there");
}

static void TestRefusals(void)
{
    mlocDateFormatter plain;
    mlocDateFormatter zoned;
    mlocDateOptions zone = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    zone.zone = mloc_zoneStyleGenericShort;
    mlocDate a = Iso(2026, 1, 5);
    mlocDate b = Iso(2026, 1, 8);
    mlocDate far = {.day = 300000000};
    char text[8];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocOutput measure = {0};
    mlocOutput broken = {.buffer = nullptr, .capacity = 4};
    CHECK(Formatter("en", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium), &plain),
          "a formatter");
    CHECK(mlocFormatDateRange(&plain, &a, &b, &measure) == mloc_errorCapacity &&
              measure.length == strlen("Jan 5" DASH "8, 2026") &&
              mlocFormatDateRange(&plain, &a, &b, &output) == mloc_errorCapacity,
          "measured, and a buffer too small");
    CHECK(mlocFormatDateRange(nullptr, &a, &b, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, nullptr, &b, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, &b, nullptr) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, &b, &broken) == mloc_errorInvalid,
          "null pointers");
    CHECK(mlocFormatDateRange(&plain, &far, &b, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, &far, &output) == mloc_errorInvalid,
          "a date outside Temporal's range at either end");
    CHECK(Formatter("en", zone, &zoned) &&
              mlocFormatDateRange(&zoned, &a, &b, &output) == mloc_errorInvalid,
          "a formatter with a zone");
    mlocDateFormatter forged;
    memset(&forged, 0x5A, sizeof(forged));
    CHECK(mlocFormatDateRange(&forged, &a, &b, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
}

// The parts' spans in UTF-16 units, as ICU counts them: the start's from
// its first part to its last, then the end's ("0:4:5,1:8:9"), or "-".
typedef struct Spans
{
    const char* text;
    long first[2];
    long last[2];
} Spans;

static long Units(const char* text, size_t bytes)
{
    long units = 0;
    for (size_t i = 0; i < bytes; i++)
    {
        uint8_t c = (uint8_t)text[i];
        units += (c & 0xC0) == 0x80 ? 0 : c >= 0xF0 ? 2 : 1;
    }
    return units;
}

static void Span(void* context, uint32_t part, size_t start, size_t length)
{
    Spans* s = context;
    uint32_t side = part & 0x300;
    if (side == 0)
    {
        return;
    }
    int i = side == mloc_dateRangeStart ? 0 : 1;
    long a = Units(s->text, start);
    long b = Units(s->text, start + length);
    s->first[i] = s->first[i] < 0 || a < s->first[i] ? a : s->first[i];
    s->last[i] = b > s->last[i] ? b : s->last[i];
}

// One line of the table: locale, the options' fields, length, year style
// and style, the start and the end (ISO year, month, day), ICU's text
// and spans, and where the library answers otherwise its text and spans
// and the cause.
static bool TableLine(char* line, size_t number)
{
    char* field[11] = {0};
    size_t count = 0;
    for (char* at = line; count < 11 && at != nullptr; count++)
    {
        field[count] = at;
        at = strchr(at, '\t');
        if (at != nullptr)
        {
            *at++ = '\0';
        }
    }
    int o[4] = {0};
    int d[6] = {0};
    if (count < 6 || sscanf(field[1], "%d %d %d %d", &o[0], &o[1], &o[2], &o[3]) != 4 ||
        sscanf(field[2], "%d-%d-%d", &d[0], &d[1], &d[2]) != 3 ||
        sscanf(field[3], "%d-%d-%d", &d[3], &d[4], &d[5]) != 3)
    {
        printf("  line %zu: unreadable\n", number);
        return false;
    }
    const char* want = count >= 9 ? field[6] : field[4];
    const char* spans = count >= 9 ? field[7] : field[5];
    mlocDateOptions options = {.fields = (mlocDateFieldSet)o[0],
                               .length = (mlocDateLength)o[1],
                               .yearStyle = (mlocYearStyle)o[2],
                               .style = (mlocDateStyle)o[3]};
    mlocDateFormatter formatter;
    mlocDate start = Iso(d[0], (uint32_t)d[1], (uint32_t)d[2]);
    mlocDate end = Iso(d[3], (uint32_t)d[4], (uint32_t)d[5]);
    char text[512];
    Spans s = {text, {-1, -1}, {0, 0}};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Span, .context = &s};
    char got[64] = "-";
    if (!Formatter(field[0], options, &formatter) ||
        mlocFormatDateRange(&formatter, &start, &end, &output) != mloc_success)
    {
        printf("  line %zu: %s not formatted\n", number, field[0]);
        return false;
    }
    if (s.first[0] >= 0 || s.first[1] >= 0)
    {
        snprintf(got, sizeof(got), "0:%ld:%ld,1:%ld:%ld", s.first[0], s.last[0], s.first[1],
                 s.last[1]);
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                strcmp(got, spans) == 0;
    if (!same)
    {
        printf("  line %zu: %s got \"%.*s\" %s, want \"%s\" %s\n", number, field[0],
               (int)output.length, text, got, want, spans);
    }
    return same;
}

static void TestTable(void)
{
    FILE* file = fopen(MLOC_DATE_RANGES_FILE, "rb");
    CHECK(file != nullptr, "the table of ICU's ranges");
    if (file == nullptr)
    {
        return;
    }
    char line[1024];
    size_t number = 0;
    size_t lines = 0;
    size_t failed = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        number++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }
        lines++;
        failed += TableLine(line, number) ? 0 : 1;
    }
    fclose(file);
    printf("%zu of %zu ranges as the table has them\n", lines - failed, lines);
    CHECK(lines > 0 && failed == 0, "every range of the table");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    if (!OpenFile(MLOC_DATES_FILE, &s_sources[1]) || !OpenFile(MLOC_SPELLOUT_FILE, &s_sources[2]) ||
        !OpenFile(MLOC_ZONE_NAMES_FILE, &s_sources[3]))
    {
        fputs("test_date_range: cannot open " MLOC_DATES_FILE ", " MLOC_SPELLOUT_FILE
              " or " MLOC_ZONE_NAMES_FILE "\n",
              stderr);
        return 2;
    }
    s_data = (mlocDataSet){s_sources, 4};
    TestDifferences();
    TestOneDateAndFallback();
    TestOwnAnswers();
    TestRefusals();
    TestTable();
    return s_failures == 0 ? 0 : 1;
}
