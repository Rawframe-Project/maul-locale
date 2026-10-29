// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Relative time (maul-locale/relative.h): amounts of each kind of unit
// in each width and mode, as ICU4C 79's RelativeDateTimeFormatter writes
// them (its formatNumeric and format), but where the library differs on
// purpose (auto without ICU's 1% epsilon; capitalization by CLDR's
// relative context transforms; parts kept); parts; refusals.

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/relative.h"

#include <stdio.h>
#include <string.h>

static mlocData s_sources[3];
static mlocDataSet s_data;

static bool OpenFile(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t read = file == nullptr ? 0 : fread(buffer, 1, capacity, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    return read > 0 && read < capacity && mlocOpenData(buffer, read, out) == mloc_success;
}

typedef struct Parts
{
    char out[512];
    const char* text;
} Parts;

static void Part(void* context, uint32_t part, size_t start, size_t length)
{
    Parts* p = context;
    size_t used = strlen(p->out);
    snprintf(p->out + used, sizeof(p->out) - used, "%u[%.*s]", part, (int)length, p->text + start);
}

static bool Formatter(const char* id, mlocRelativeOptions options, mlocRelativeFormatter* f)
{
    mlocLocale locale;
    return mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status ==
               mloc_success &&
           mlocInitRelativeFormatter(&s_data, &locale, &options, f) >= 0;
}

// Whether an amount of a unit writes want (and the parts, when given).
static bool Is(const char* id, mlocRelativeOptions options, const char* amount,
               mlocRelativeUnit unit, const char* want, const char* parts)
{
    mlocRelativeFormatter f;
    mlocDecimal d;
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    if (!Formatter(id, options, &f) ||
        mlocParseDecimal(amount, strlen(amount), &d).status != mloc_success ||
        mlocRelativeFormatter_Format(&f, &d, unit, &output) != mloc_success)
    {
        printf("  %s %s: not formatted\n", id, amount);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s %s: got \"%.*s\" %s, want \"%s\"\n", id, amount, (int)output.length, text,
               p.out, want);
    }
    return same;
}

static mlocRelativeOptions Options(mlocRelativeWidth width, mlocRelativeNumeric numeric)
{
    return (mlocRelativeOptions){.width = width, .numeric = numeric};
}

static void TestNumeric(void)
{
    mlocRelativeOptions always = Options(mloc_relativeWidthLong, mloc_relativeNumericAlways);
    CHECK(Is("en", always, "3", mloc_relativeDay, "in 3 days", "13[in ]0[3]13[ days]"),
          "the future's parts: literal, integer, literal");
    CHECK(Is("en", always, "-1", mloc_relativeDay, "1 day ago", nullptr) &&
              Is("en", always, "-0", mloc_relativeDay, "0 days ago", nullptr),
          "the past, -0 included, the category of the magnitude");
    CHECK(Is("en", always, "1.2", mloc_relativeDay, "in 1.2 days", nullptr) &&
              Is("pl", always, "5", mloc_relativeDay, "za 5 dni", nullptr) &&
              Is("pl", always, "1.5", mloc_relativeDay, "za 1,5 dnia", nullptr),
          "plural categories of the number as written");
    CHECK(Is("en", Options(mloc_relativeWidthShort, mloc_relativeNumericAlways), "-2",
             mloc_relativeHour, "2 hr. ago", nullptr) &&
              Is("en", Options(mloc_relativeWidthNarrow, mloc_relativeNumericAlways), "2",
                 mloc_relativeHour, "in 2h", nullptr) &&
              Is("en", Options(mloc_relativeWidthNarrow, mloc_relativeNumericAlways), "3",
                 mloc_relativeYear, "in 3y", nullptr),
          "the short and narrow widths");
    CHECK(Is("ar", always, "1", mloc_relativeYear, "خلال سنة واحدة", "13[خلال سنة واحدة]") &&
              Is("ar", always, "2", mloc_relativeYear, "خلال سنتين", nullptr),
          "a pattern without the number writes none");
    CHECK(Is("en", always, "1", mloc_relativeMonday, "in 1 Monday", nullptr),
          "a weekday's numeric pattern");
    CHECK(Is("ha", always, "3", mloc_relativeDay, "a cikin kwanaki 3", "13[a cikin kwanaki ]0[3]"),
          "a pattern ending in its number");
    mlocRelativeOptions compact = always;
    compact.number.notation = mloc_notationCompactShort;
    CHECK(Is("en", compact, "1000000", mloc_relativeDay, "in 1M days", nullptr), "compact short");
    compact.number.notation = mloc_notationCompactLong;
    CHECK(Is("en", compact, "1000000", mloc_relativeDay, "in 1 million days", nullptr),
          "compact long");
}

static void TestAuto(void)
{
    mlocRelativeOptions automatic = Options(mloc_relativeWidthLong, mloc_relativeNumericAuto);
    CHECK(Is("en", automatic, "-1", mloc_relativeDay, "yesterday", "13[yesterday]") &&
              Is("en", automatic, "1.0", mloc_relativeDay, "tomorrow", nullptr) &&
              Is("en", automatic, "-0", mloc_relativeDay, "today", nullptr),
          "named amounts: -1, 1.0 as 1, -0 as 0");
    CHECK(Is("es", automatic, "-2", mloc_relativeDay, "anteayer", nullptr) &&
              Is("en", automatic, "2", mloc_relativeDay, "in 2 days", nullptr),
          "-2 where the locale names it, the number where it does not");
    CHECK(Is("en", automatic, "0", mloc_relativeSecond, "now", nullptr) &&
              Is("en", automatic, "1", mloc_relativeMonday, "next Monday", nullptr) &&
              Is("en", automatic, "3", mloc_relativeDay, "in 3 days", nullptr),
          "now, next Monday, and an amount without a name");
    // ICU4C takes 1.004 within its 1% epsilon as 1 ("tomorrow").
    CHECK(Is("en", automatic, "1.004", mloc_relativeDay, "in 1.004 days", nullptr),
          "no epsilon, as ECMA-402");
}

static void TestCapitalization(void)
{
    mlocRelativeOptions o = Options(mloc_relativeWidthLong, mloc_relativeNumericAuto);
    o.capitalization = mloc_capitalizationSentence;
    CHECK(Is("en", o, "-1", mloc_relativeDay, "Yesterday", "13[Yesterday]") &&
              Is("en", o, "3", mloc_relativeDay, "In 3 days", "13[In ]0[3]13[ days]") &&
              Is("en", o, "-3", mloc_relativeDay, "3 days ago", nullptr),
          "a sentence's start, parts kept, a number first left");
    o.capitalization = mloc_capitalizationListOrMenu;
    CHECK(Is("en", o, "1", mloc_relativeDay, "Tomorrow", nullptr) &&
              Is("de", o, "1", mloc_relativeDay, "morgen", nullptr),
          "a menu by the locale's relative transforms");
    o.capitalization = mloc_capitalizationStandAlone;
    CHECK(Is("en", o, "1", mloc_relativeDay, "Tomorrow", nullptr) &&
              Is("de", o, "1", mloc_relativeDay, "morgen", nullptr),
          "standing alone by the locale's relative transforms");
    o.capitalization = mloc_capitalizationNone;
    CHECK(Is("en", o, "1", mloc_relativeDay, "tomorrow", nullptr), "none");
    o.capitalization = mloc_capitalizationSentence;
    CHECK(Is("tr", o, "1", mloc_relativeDay, "Yarın", nullptr), "Turkish");
}

static void TestRefusals(void)
{
    mlocRelativeFormatter f;
    mlocLocale en;
    mlocRelativeOptions bad = {.number = {.style = mloc_numberPercent}};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitRelativeFormatter(&s_data, &en, &bad, &f) == mloc_errorInvalid,
          "a percent style");
    bad = (mlocRelativeOptions){.number = {.signDisplay = mloc_signAlways}};
    CHECK(mlocInitRelativeFormatter(&s_data, &en, &bad, &f) == mloc_errorInvalid, "a sign display");
    bad = (mlocRelativeOptions){.width = (mlocRelativeWidth)3};
    CHECK(mlocInitRelativeFormatter(&s_data, &en, &bad, &f) == mloc_errorInvalid &&
              mlocInitRelativeFormatter(&s_data, &en, nullptr, nullptr) == mloc_errorInvalid,
          "a width out of range, a null formatter");
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet alone = {&builtin, 1};
    CHECK(mlocInitRelativeFormatter(&alone, &en, nullptr, &f) == mloc_errorNotFound,
          "data without relative time");
    mlocDecimal one;
    // A decimal that is not finite has no digits.
    mlocDecimal infinity;
    memset(&infinity, 0, sizeof(infinity));
    infinity.kind = mloc_decimalInfinity;
    char text[8];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocInitRelativeFormatter(&s_data, &en, nullptr, &f) == mloc_success &&
              mlocMakeDecimal(1, &one) == mloc_success &&
              mlocRelativeFormatter_Format(&f, &infinity, mloc_relativeDay, &output) ==
                  mloc_errorInvalid &&
              mlocRelativeFormatter_Format(&f, &one, (mlocRelativeUnit)15, &output) ==
                  mloc_errorInvalid,
          "an amount not finite, a unit out of range");
    output = (mlocOutput){.buffer = text, .capacity = 4};
    CHECK(mlocRelativeFormatter_Format(&f, &one, mloc_relativeDay, &output) == mloc_errorCapacity &&
              output.length == strlen("in 1 day"),
          "the length needed when the text does not fit");
    mlocRelativeFormatter named;
    mlocRelativeOptions automatic = {.numeric = mloc_relativeNumericAuto};
    mlocDecimal minusOne;
    char exact[9];
    output = (mlocOutput){.buffer = exact, .capacity = sizeof(exact)};
    CHECK(mlocInitRelativeFormatter(&s_data, &en, &automatic, &named) == mloc_success &&
              mlocMakeDecimal(-1, &minusOne) == mloc_success &&
              mlocRelativeFormatter_Format(&named, &minusOne, mloc_relativeDay, &output) ==
                  mloc_success &&
              output.length == 9 && memcmp(exact, "yesterday", 9) == 0,
          "a name that fills the room exactly");
    output = (mlocOutput){.buffer = exact, .capacity = 8};
    CHECK(mlocRelativeFormatter_Format(&named, &minusOne, mloc_relativeDay, &output) ==
                  mloc_errorCapacity &&
              output.length == 9,
          "a name one byte past the room");
    mlocRelativeFormatter forged = f;
    memset(&forged, 0x5A, sizeof(forged));
    output = (mlocOutput){.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocRelativeFormatter_Format(&forged, &one, mloc_relativeDay, &output) ==
              mloc_errorInvalid,
          "a formatter the library did not make");
}

// Whether a relative day and a time write want (and the parts, when
// given).
static bool DayTimeIs(const char* id, mlocRelativeOptions options, const char* days, uint8_t hour,
                      uint8_t minute, const char* want, const char* parts)
{
    mlocLocale locale;
    mlocRelativeFormatter relative;
    mlocTimeFormatter time;
    mlocTimeOptions timeOptions = {.precision = mloc_timePrecisionMinute};
    mlocDecimal d;
    mlocTime at = {.hour = hour, .minute = minute};
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    if (mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status != mloc_success ||
        mlocInitRelativeFormatter(&s_data, &locale, &options, &relative) < 0 ||
        mlocInitTimeFormatter(&s_data, &locale, &timeOptions, &time) < 0 ||
        mlocParseDecimal(days, strlen(days), &d).status != mloc_success ||
        mlocRelativeFormatter_FormatDayTime(&relative, &time, &d, &at, &output) != mloc_success)
    {
        printf("  %s %s: not formatted\n", id, days);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s %s: got \"%.*s\" %s, want \"%s\"\n", id, days, (int)output.length, text, p.out,
               want);
    }
    return same;
}

#define NNBSP "\xE2\x80\xAF"

static void TestDayTime(void)
{
    mlocRelativeOptions automatic = Options(mloc_relativeWidthLong, mloc_relativeNumericAuto);
    CHECK(DayTimeIs("en", automatic, "1", 15, 0, "tomorrow at 3:00" NNBSP "PM",
                    "14[tomorrow]0[ at ]8[3]0[:]9[00]0[" NNBSP "]12[PM]"),
          "the long relative glue, the day whole as one part");
    CHECK(DayTimeIs("en", Options(mloc_relativeWidthShort, mloc_relativeNumericAuto), "-1", 9, 30,
                    "yesterday, 9:30" NNBSP "AM", nullptr) &&
              DayTimeIs("en", Options(mloc_relativeWidthLong, mloc_relativeNumericAlways), "3", 9,
                        30, "in 3 days at 9:30" NNBSP "AM", nullptr),
          "the medium glue for short, a numeric day");
    automatic.capitalization = mloc_capitalizationSentence;
    CHECK(DayTimeIs("en", automatic, "0", 15, 0, "Today at 3:00" NNBSP "PM", nullptr),
          "the day capitalized where it comes first");
    CHECK(DayTimeIs("gd", automatic, "1", 15, 0, "15:00 a-màireach", nullptr),
          "a day after the time is not capitalized");
}

// The time formatter's hour pattern at a whole hour, its own otherwise.
static void TestDayTimeHour(void)
{
    mlocLocale en;
    mlocRelativeFormatter relative;
    mlocTimeFormatter time;
    mlocTimeOptions optional = {.precision = mloc_timePrecisionMinuteOptional};
    mlocDecimal one;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitRelativeFormatter(&s_data, &en, nullptr, &relative) == mloc_success &&
              mlocInitTimeFormatter(&s_data, &en, &optional, &time) >= 0 &&
              mlocMakeDecimal(1, &one) == mloc_success &&
              mlocRelativeFormatter_FormatDayTime(&relative, &time, &one, &(mlocTime){.hour = 15},
                                                  &output) == mloc_success &&
              output.length == strlen("in 1 day at 3" NNBSP "PM") &&
              memcmp(text, "in 1 day at 3" NNBSP "PM", output.length) == 0,
          "a whole hour without its minutes");
    CHECK(mlocRelativeFormatter_FormatDayTime(&relative, &time, &one,
                                              &(mlocTime){.hour = 15, .minute = 30},
                                              &output) == mloc_success &&
              output.length == strlen("in 1 day at 3:30" NNBSP "PM") &&
              memcmp(text, "in 1 day at 3:30" NNBSP "PM", output.length) == 0,
          "a time with its minutes");
}

static void TestDayTimeRefusals(void)
{
    mlocLocale en;
    mlocRelativeFormatter relative;
    mlocTimeFormatter time;
    mlocDecimal one;
    mlocTime noon = {.hour = 12};
    mlocTime bad = {.hour = 24};
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitRelativeFormatter(&s_data, &en, nullptr, &relative) == mloc_success &&
              mlocInitTimeFormatter(&s_data, &en, nullptr, &time) >= 0 &&
              mlocMakeDecimal(1, &one) == mloc_success,
          "formatters made");
    CHECK(mlocRelativeFormatter_FormatDayTime(&relative, &time, &one, &bad, &output) ==
                  mloc_errorInvalid &&
              mlocRelativeFormatter_FormatDayTime(nullptr, &time, &one, &noon, &output) ==
                  mloc_errorInvalid &&
              mlocRelativeFormatter_FormatDayTime(&relative, &time, nullptr, &noon, &output) ==
                  mloc_errorInvalid,
          "a time out of range, null pointers");
    output = (mlocOutput){.buffer = text, .capacity = 5};
    CHECK(mlocRelativeFormatter_FormatDayTime(&relative, &time, &one, &noon, &output) ==
                  mloc_errorCapacity &&
              output.length > 5,
          "the length needed when the text does not fit");
    mlocRelativeOptions coarse = {.number = {.precision = {.kind = mloc_precisionSignificant,
                                                           .minSignificantDigits = 1,
                                                           .maxSignificantDigits = 1}}};
    mlocDecimal huge;
    output = (mlocOutput){.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocInitRelativeFormatter(&s_data, &en, &coarse, &relative) == mloc_success &&
              mlocParseDecimal("9.9e999999", 10, &huge).status == mloc_success &&
              mlocRelativeFormatter_FormatDayTime(&relative, &time, &huge, &noon, &output) ==
                  mloc_errorLimit,
          "a day rounded past the decimal's limits");
}

int main(void)
{
    alignas(8) static uint8_t s_file[1 << 20];
    alignas(8) static uint8_t s_dates[1 << 21];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 3};
    if (!OpenFile(MLOC_RELATIVE_FILE, s_file, sizeof(s_file), &s_sources[1]) ||
        !OpenFile(MLOC_DATES_FILE, s_dates, sizeof(s_dates), &s_sources[2]))
    {
        fputs("test_relative: cannot open " MLOC_RELATIVE_FILE " or " MLOC_DATES_FILE "\n", stderr);
        return 2;
    }
    TestNumeric();
    TestAuto();
    TestCapitalization();
    TestRefusals();
    TestDayTime();
    TestDayTimeHour();
    TestDayTimeRefusals();
    return s_failures == 0 ? 0 : 1;
}
