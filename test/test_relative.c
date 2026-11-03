// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Relative time (maul-locale/relative.h): amounts of each kind of unit
// in each width and mode, as ICU4C 79's RelativeDateTimeFormatter writes
// them (its formatNumeric and format), but where the library differs on
// purpose (auto without ICU's 1% epsilon; capitalization by CLDR's
// relative context transforms; parts kept); parts; refusals. Then a name
// and a pattern of any length, from a pack before the data.

#include "kinds.h"
#include "relative_tables.h"
#include "test_harness.h"
#include "test_pack.h"

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

static void Part(void* context, mlocPart part, size_t start, size_t length)
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

// The first part reported, and how many there were.
typedef struct FirstPart
{
    mlocPart part;
    size_t start;
    size_t length;
    size_t count;
} FirstPart;

static void RecordPart(void* context, mlocPart part, size_t start, size_t length)
{
    FirstPart* first = context;
    if (first->count++ == 0)
    {
        *first = (FirstPart){part, start, length, 1};
    }
}

// A day whose text is long: 1e1600 days take 2142 bytes in English.
static void TestDayTimeLong(void)
{
    mlocLocale en;
    mlocRelativeFormatter relative;
    mlocTimeFormatter time;
    mlocTimeOptions optional = {.precision = mloc_timePrecisionMinuteOptional};
    mlocDecimal days;
    static char day[4096];
    static char text[4096];
    const char* glue = " at 3" NNBSP "PM";
    FirstPart first = {0};
    mlocOutput dayOutput = {.buffer = day, .capacity = sizeof(day)};
    mlocOutput output = {
        .buffer = text, .capacity = sizeof(text), .part = RecordPart, .context = &first};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitRelativeFormatter(&s_data, &en, nullptr, &relative) == mloc_success &&
              mlocInitTimeFormatter(&s_data, &en, &optional, &time) >= 0 &&
              mlocParseDecimal("1e1600", 6, &days).status == mloc_success &&
              mlocRelativeFormatter_Format(&relative, &days, mloc_relativeDay, &dayOutput) ==
                  mloc_success &&
              dayOutput.length == 2142 &&
              mlocRelativeFormatter_FormatDayTime(&relative, &time, &days, &(mlocTime){.hour = 15},
                                                  &output) == mloc_success &&
              output.length == dayOutput.length + strlen(glue) &&
              memcmp(text, day, dayOutput.length) == 0 &&
              memcmp(text + dayOutput.length, glue, strlen(glue)) == 0,
          "a day of 2142 bytes is written whole before the time");
    CHECK(first.part == mloc_datePartRelativeDay && first.start == 0 &&
              first.length == dayOutput.length && first.count > 1,
          "as one part");
}

// Opens a pack whose root gives day, long, a name for +1 of length
// "e"s and an other future pattern "in {0}" then length "f"s, before
// the data; false when it is refused.
static bool OpenLongTexts(uint32_t length, mlocData* sources)
{
    static uint8_t s_unit[4096];
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t day = relativeDay * relativeWidthCount * relativeSlots;
    uint32_t size = 2 * length + 6;
    uint8_t* keys = s_unit + relativeHeaderSize;
    uint8_t* offsets = keys + 2 * 2 * 2;
    uint8_t* text = offsets + 3 * 4;
    PackPut(s_unit, 2, 4);
    PackPut(s_unit + 4, 2, 4);
    PackPut(s_unit + 8, size, 4);
    PackPut(keys, day + 3, 2);
    PackPut(keys + 2, day + relativeFuture + 5, 2);
    PackPut(keys + 6, 1, 2);
    PackPut(offsets + 4, length, 4);
    PackPut(offsets + 8, size, 4);
    memset(text, 'e', length);
    memcpy(text + length, "in {0}", 6);
    memset(text + length + 6, 'f', length);
    PackUnit unit = {kindRelative, {s_unit, (uint32_t)(text + size - s_unit)}, true};
    memcpy(sources + 1, s_sources, sizeof(s_sources));
    return OpenRootPack(&unit, 1, &sources[0]);
}

// Names and patterns are written from the data whatever their length.
static void TestLongTexts(void)
{
    mlocData sources[4];
    mlocDataSet set = {sources, 4};
    mlocLocale und;
    mlocRelativeFormatter named;
    mlocRelativeFormatter numeric;
    mlocTimeFormatter time;
    mlocRelativeOptions automatic = {.numeric = mloc_relativeNumericAuto};
    mlocDecimal one;
    static char text[4096];
    static char want[4096];
    FirstPart first = {0};
    CHECK(OpenLongTexts(300, sources) &&
              mlocCanonicalizeLocale(&set, "und", 3, &und, nullptr).status == mloc_success &&
              mlocInitRelativeFormatter(&set, &und, &automatic, &named) == mloc_success &&
              mlocInitRelativeFormatter(&set, &und, nullptr, &numeric) == mloc_success &&
              mlocInitTimeFormatter(&set, &und, nullptr, &time) >= 0 &&
              mlocMakeDecimal(1, &one) == mloc_success,
          "formatters on a pack of 300-byte texts");
    mlocOutput output = {
        .buffer = text, .capacity = sizeof(text), .part = RecordPart, .context = &first};
    memset(want, 'e', 300);
    CHECK(mlocRelativeFormatter_Format(&named, &one, mloc_relativeDay, &output) == mloc_success &&
              output.length == 300 && memcmp(text, want, 300) == 0 &&
              first.part == mloc_numberPartLiteral && first.length == 300 && first.count == 1,
          "a name of 300 bytes as one literal part");
    first = (FirstPart){0};
    output = (mlocOutput){
        .buffer = text, .capacity = sizeof(text), .part = RecordPart, .context = &first};
    memcpy(want, "in 1", 4);
    memset(want + 4, 'f', 300);
    CHECK(mlocRelativeFormatter_Format(&numeric, &one, mloc_relativeDay, &output) == mloc_success &&
              output.length == 304 && memcmp(text, want, 304) == 0 &&
              first.part == mloc_numberPartLiteral && first.length == 3 && first.count == 3,
          "a pattern of 303 bytes around the number");
    first = (FirstPart){0};
    output = (mlocOutput){
        .buffer = text, .capacity = sizeof(text), .part = RecordPart, .context = &first};
    memset(want, 'e', 300);
    CHECK(mlocRelativeFormatter_FormatDayTime(&named, &time, &one, &(mlocTime){.hour = 12},
                                              &output) == mloc_success &&
              output.length > 300 && memcmp(text, want, 300) == 0 &&
              first.part == mloc_datePartRelativeDay && first.length == 300,
          "the name before a time, as the day's part");
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
    // The glue writes " at " and the time around the day: none of it
    // reaches the output.
    memset(text, 'X', sizeof(text));
    output = (mlocOutput){.buffer = text, .capacity = sizeof(text), .length = 99};
    bool kept = true;
    CHECK(mlocInitRelativeFormatter(&s_data, &en, &coarse, &relative) == mloc_success &&
              mlocParseDecimal("9.9e999999", 10, &huge).status == mloc_success &&
              mlocRelativeFormatter_FormatDayTime(&relative, &time, &huge, &noon, &output) ==
                  mloc_errorLimit,
          "a day rounded past the decimal's limits");
    for (size_t i = 0; i < sizeof(text); i++)
    {
        kept = kept && text[i] == 'X';
    }
    CHECK(kept && output.length == 99, "is refused with the output left as it was");
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
    TestDayTimeLong();
    TestLongTexts();
    TestDayTimeRefusals();
    return s_failures == 0 ? 0 : 1;
}
