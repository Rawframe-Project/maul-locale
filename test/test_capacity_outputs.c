// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A text one byte too long for its buffer: every formatting call that
// writes through a writer gives mloc_errorCapacity with the length needed
// and leaves the buffer as it was (base.h); one that just fits writes the
// same text and nothing past it. Texts longer than the library's scratch
// take the second pass, straight into the buffer.

#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/currency.h"
#include "maul-locale/datetime.h"
#include "maul-locale/decimal.h"
#include "maul-locale/duration.h"
#include "maul-locale/grammar.h"
#include "maul-locale/ixdtf.h"
#include "maul-locale/list.h"
#include "maul-locale/message.h"
#include "maul-locale/names.h"
#include "maul-locale/number.h"
#include "maul-locale/unit.h"
#include "maul-locale/zone.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static const char* const s_files[] = {
    "dates.mloc", "names.mloc", "spellout.mloc", "units.mloc", "zone-names.mloc",
};
enum
{
    fileCount = sizeof(s_files) / sizeof(s_files[0]),
};

static mlocData s_sources[fileCount + 1];
static mlocDataSet s_data;
static mlocLocale s_en;
static mlocLocale s_tr;
static mlocDecimal s_amount;
static mlocDecimal s_end;
static mlocDate s_date;
static mlocDate s_later;
static mlocTime s_time;
static mlocTime s_evening;
static mlocDateTime s_dateTime;
static mlocDateTime s_laterDateTime;
static mlocTimeZone s_zone;
static mlocNumberFormatter s_number;
static mlocCurrencyFormatter s_currency;
static mlocUnitFormatter s_unit;
static mlocDateFormatter s_dates;
static mlocTimeFormatter s_times;
static mlocDateTimeFormatter s_dateTimes;
static mlocZoneFormatter s_zones;
static mlocListFormatter s_list;
static mlocDurationFormatter s_durations;
static mlocInflector s_turkish;
static mlocListItem s_items[3];
static mlocListItem s_longItems[3];
static char s_longWord[200];
static mlocLocale s_longLocale;
static mlocMessageFormatter s_messages;
static uint8_t s_bound[2][4096];
static char s_longCalendar[160];

// A call writing its text into an output.
typedef mlocResult (*Call)(mlocOutput* output);

static mlocResult Number(mlocOutput* o)
{
    return mlocNumberFormatter_Format(&s_number, &s_amount, o);
}

static mlocResult NumberRange(mlocOutput* o)
{
    return mlocNumberFormatter_FormatRange(&s_number, &s_amount, &s_end, o);
}

static mlocResult Currency(mlocOutput* o)
{
    return mlocCurrencyFormatter_Format(&s_currency, &s_amount, o);
}

static mlocResult Unit(mlocOutput* o)
{
    return mlocUnitFormatter_Format(&s_unit, &s_amount, o);
}

static mlocResult Date(mlocOutput* o)
{
    return mlocDateFormatter_Format(&s_dates, &s_date, o);
}

static mlocResult DateRange(mlocOutput* o)
{
    return mlocDateFormatter_FormatRange(&s_dates, &s_date, &s_later, o);
}

static mlocResult Time(mlocOutput* o)
{
    return mlocTimeFormatter_Format(&s_times, &s_time, o);
}

static mlocResult TimeRange(mlocOutput* o)
{
    return mlocTimeFormatter_FormatRange(&s_times, &s_time, &s_evening, o);
}

static mlocResult DateTime(mlocOutput* o)
{
    return mlocDateTimeFormatter_Format(&s_dateTimes, &s_dateTime, o);
}

static mlocResult DateTimeRange(mlocOutput* o)
{
    return mlocDateTimeFormatter_FormatRange(&s_dateTimes, &s_dateTime, &s_laterDateTime, o);
}

static mlocResult ZoneName(mlocOutput* o)
{
    static const mlocInstant s_july = {1782864000, 0};
    return mlocZoneFormatter_Format(&s_zones, &s_zone, &s_july, o);
}

static mlocResult List(mlocOutput* o)
{
    return mlocListFormatter_Format(&s_list, s_items, 3, o);
}

static mlocResult LongList(mlocOutput* o)
{
    return mlocListFormatter_Format(&s_list, s_longItems, 3, o);
}

static mlocResult Duration(mlocOutput* o)
{
    static const mlocDurationValues s_values = {.hours = 1, .minutes = 30, .seconds = 15};
    return mlocDurationFormatter_Format(&s_durations, &s_values, o);
}

static mlocResult Inflection(mlocOutput* o)
{
    static const mlocInflection s_dative = {.grammaticalCase = mloc_caseDative};
    return mlocInflector_InflectText(&s_turkish, "kitap", 5, &s_dative, o);
}

static mlocResult LongInflection(mlocOutput* o)
{
    static const mlocInflection s_dative = {.grammaticalCase = mloc_caseDative};
    return mlocInflector_InflectText(&s_turkish, s_longWord, sizeof(s_longWord), &s_dative, o);
}

static mlocResult NumberInflection(mlocOutput* o)
{
    static const mlocInflection s_dative = {.grammaticalCase = mloc_caseDative};
    return mlocInflector_InflectNumber(&s_turkish, nullptr, &s_amount, &s_dative, o);
}

static mlocResult Name(mlocOutput* o)
{
    static const mlocNameOptions s_title = {.capitalization = mloc_capitalizationStandAlone};
    return mlocGetName(&s_data, &s_tr, mloc_nameLanguage, "fr", 2, &s_title, o);
}

static mlocResult LocaleName(mlocOutput* o)
{
    return mlocGetLocaleName(&s_data, &s_en, &s_tr, nullptr, o);
}

static mlocResult LongLocaleName(mlocOutput* o)
{
    return mlocGetLocaleName(&s_data, &s_en, &s_longLocale, nullptr, o);
}

static mlocResult Message(mlocOutput* o)
{
    static const mlocMessageArgument s_count = {
        "n", 1, {mloc_messageValueInteger, 0, {.integer = 3}}};
    return mlocFormatMessage(s_bound[0], &s_count, 1, o, nullptr);
}

static mlocResult LongMessage(mlocOutput* o)
{
    return mlocFormatMessage(s_bound[1], nullptr, 0, o, nullptr);
}

static mlocResult TemporalDate(mlocOutput* o)
{
    return mlocFormatTemporalDate(&s_date, nullptr, o);
}

static mlocResult LongTemporalDate(mlocOutput* o)
{
    mlocTemporalStringOptions options = {.calendar = s_longCalendar,
                                         .calendarLength = sizeof(s_longCalendar) - 1};
    return mlocFormatTemporalDate(&s_date, &options, o);
}

static mlocResult TemporalDuration(mlocOutput* o)
{
    static const mlocDurationValues s_values = {
        .days = 3, .hours = 4, .seconds = 5, .milliseconds = 250};
    return mlocFormatTemporalDuration(&s_values, nullptr, o);
}

static mlocResult ZonedString(mlocOutput* o)
{
    static const mlocInstant s_july = {1782864000, 123456789};
    return mlocFormatZonedDateTime(&s_zone, &s_july, nullptr, o);
}

typedef struct Case
{
    const char* name;
    Call call;
    // The scratch the text must pass to take the second pass (128 bytes
    // for a writer's, 256 for a name's), or 0.
    size_t past;
} Case;

#define CASE(call, past) {#call, call, past}

static const Case s_cases[] = {
    CASE(Number, 0),
    CASE(NumberRange, 0),
    CASE(Currency, 0),
    CASE(Unit, 0),
    CASE(Date, 0),
    CASE(DateRange, 0),
    CASE(Time, 0),
    CASE(TimeRange, 0),
    CASE(DateTime, 0),
    CASE(DateTimeRange, 0),
    CASE(ZoneName, 0),
    CASE(List, 0),
    CASE(LongList, 128),
    CASE(Duration, 0),
    CASE(Inflection, 0),
    CASE(LongInflection, 128),
    CASE(NumberInflection, 0),
    CASE(Name, 0),
    CASE(LocaleName, 0),
    CASE(LongLocaleName, 256),
    CASE(Message, 0),
    CASE(LongMessage, 512),
    CASE(TemporalDate, 0),
    CASE(LongTemporalDate, 128),
    CASE(TemporalDuration, 0),
    CASE(ZonedString, 0),
};

static bool Kept(const char* bytes, size_t from, size_t to)
{
    for (size_t i = from; i < to; i++)
    {
        if ((unsigned char)bytes[i] != pattern)
        {
            return false;
        }
    }
    return true;
}

static void TestCapacity(void)
{
    static char s_whole[1024];
    static char s_room[1024];
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        const Case* c = &s_cases[i];
        mlocOutput whole = {.buffer = s_whole, .capacity = sizeof(s_whole)};
        mlocResult made = c->call(&whole);
        size_t n = whole.length;
        bool sized = made >= 0 && n > 0 && n < sizeof(s_room) && n > c->past;
        memset(s_room, pattern, sizeof(s_room));
        mlocOutput cut = {.buffer = s_room, .capacity = sized ? n - 1 : 0};
        mlocResult refused = c->call(&cut);
        bool kept =
            refused == mloc_errorCapacity && cut.length == n && Kept(s_room, 0, sizeof(s_room));
        mlocOutput fits = {.buffer = s_room, .capacity = n};
        mlocResult again = c->call(&fits);
        bool fit = again == made && fits.length == n && memcmp(s_room, s_whole, n) == 0 &&
                   Kept(s_room, n, sizeof(s_room));
        if (!(sized && kept && fit))
        {
            printf("  %s: made %d length %zu, refused %d length %zu kept %d, again %d\n", c->name,
                   made, n, refused, cut.length, kept, again);
        }
        CHECK(sized && kept && fit,
              "a text one byte too long leaves the buffer and gives the length; one that fits "
              "is the same text");
    }
}

static bool OpenData(void)
{
    // Room for every file at once.
    alignas(8) static uint8_t s_bytes[16 << 20];
    size_t used = 0;
    mlocOpenBuiltinData(&s_sources[0]);
    for (size_t i = 0; i < fileCount; i++)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", MLOC_GENERATED_DIR, s_files[i]);
        FILE* file = fopen(path, "rb");
        size_t size = file == nullptr ? 0 : fread(s_bytes + used, 1, sizeof(s_bytes) - used, file);
        if (file != nullptr)
        {
            fclose(file);
        }
        if (size == 0 || used + size == sizeof(s_bytes) ||
            mlocOpenData(s_bytes + used, size, &s_sources[i + 1]) != mloc_success)
        {
            fprintf(stderr, "test_capacity_outputs: cannot open %s\n", path);
            return false;
        }
        used += (size + 7) & ~(size_t)7;
    }
    s_data = (mlocDataSet){s_sources, fileCount + 1};
    return true;
}

// Compiles and binds a message into s_bound[slot]; the bound message
// points into its compiled one, which each slot keeps.
static bool Bind(const char* text, size_t length, size_t slot)
{
    static uint8_t s_compiled[2][4096];
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    return mlocCompileMessage(text, length, s_compiled[slot], sizeof(s_compiled[slot]), &size,
                              &error)
                   .status == mloc_success &&
           mlocMessageFormatter_Bind(&s_messages, s_compiled[slot], size, s_bound[slot],
                                     sizeof(s_bound[slot]), &bound) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        return 1;
    }
    static const char s_longLocaleId[] =
        "en-Latn-US-u-ca-buddhist-co-phonebk-cu-eur-fw-wed-hc-h23-ka-shifted-kf-upper-"
        "kn-ms-uksystem-nu-arab-sd-usca-tz-usnyc";
    static char s_longText[3][100];
    for (size_t i = 0; i < 3; i++)
    {
        memset(s_longText[i], (int)('a' + i), sizeof(s_longText[i]));
        s_longItems[i] = (mlocListItem){s_longText[i], sizeof(s_longText[i])};
    }
    s_items[0] = (mlocListItem){"red", 3};
    s_items[1] = (mlocListItem){"green", 5};
    s_items[2] = (mlocListItem){"blue", 4};
    memset(s_longWord, 'a', sizeof(s_longWord));
    s_longWord[sizeof(s_longWord) - 1] = 'k';
    mlocDateInput day = {.year = 2026, .month = 10, .day = 31};
    mlocDateInput later = {.year = 2026, .month = 12, .day = 24};
    mlocTimeInput noon = {.hour = 12, .minute = 34};
    mlocTimeInput evening = {.hour = 19, .minute = 5};
    mlocDateOptions longDate = {.length = mloc_dateLengthLong};
    mlocTimeOptions shortTime = {.style = mloc_timeStyleShort};
    mlocDateTimeOptions full = {
        .date = {.fields = mloc_dateFieldSetYearMonthDay, .length = mloc_dateLengthLong},
        .time = {.precision = mloc_timePrecisionMinute},
    };
    mlocUnitOptions longUnit = {.width = mloc_unitWidthLong};
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success &&
              mlocParseLocale("tr", 2, &s_tr).status == mloc_success &&
              mlocCanonicalizeLocale(&s_data, s_longLocaleId, sizeof(s_longLocaleId) - 1,
                                     &s_longLocale, nullptr)
                      .status == mloc_success &&
              mlocParseDecimal("1234.5", 6, &s_amount).status == mloc_success &&
              mlocParseDecimal("5678.25", 7, &s_end).status == mloc_success &&
              mlocMakeDate(mloc_calendarGregorian, &day, mloc_overflowReject, &s_date) ==
                  mloc_success &&
              mlocMakeDate(mloc_calendarGregorian, &later, mloc_overflowReject, &s_later) ==
                  mloc_success &&
              mlocMakeTime(&noon, mloc_overflowReject, &s_time) == mloc_success &&
              mlocMakeTime(&evening, mloc_overflowReject, &s_evening) == mloc_success &&
              mlocMakeDateTime(&s_date, &s_time, &s_dateTime) == mloc_success &&
              mlocMakeDateTime(&s_later, &s_evening, &s_laterDateTime) == mloc_success &&
              mlocFindTimeZone(&s_data, "America/New_York", 16, &s_zone) == mloc_success &&
              mlocInitNumberFormatter(&s_data, &s_en, nullptr, &s_number) == mloc_success &&
              mlocInitCurrencyFormatter(&s_data, &s_en, "EUR", 3, nullptr, &s_currency) ==
                  mloc_success &&
              mlocInitUnitFormatter(&s_data, &s_en, "kilometer-per-hour", 18, &longUnit, &s_unit) ==
                  mloc_success &&
              mlocInitDateFormatter(&s_data, &s_en, &longDate, &s_dates) == mloc_success &&
              mlocInitTimeFormatter(&s_data, &s_en, &shortTime, &s_times) == mloc_success &&
              mlocInitDateTimeFormatter(&s_data, &s_en, &full, &s_dateTimes) == mloc_success &&
              mlocInitZoneFormatter(&s_data, &s_en, mloc_zoneStyleGenericLong, &s_zones) ==
                  mloc_success &&
              mlocInitListFormatter(&s_data, &s_en, nullptr, &s_list) == mloc_success &&
              mlocInitDurationFormatter(&s_data, &s_en, nullptr, &s_durations) == mloc_success &&
              mlocInitInflector(&s_data, &s_tr, &s_turkish) == mloc_success,
          "formatters and values to write");
    static const char s_count[] = ".input {$n :integer} .match $n one {{You have {$n} new "
                                  "message.}} * {{You have {$n} new messages.}}";
    static char s_long[600];
    memset(s_long, 'x', sizeof(s_long));
    // Components of eight letters joined by hyphens, as the grammar has
    // them.
    for (size_t i = 0; i + 1 < sizeof(s_longCalendar); i++)
    {
        s_longCalendar[i] = i % 9 == 8 ? '-' : (char)('a' + i % 9);
    }
    CHECK(mlocInitMessageFormatter(&s_data, &s_en, nullptr, &s_messages) == mloc_success &&
              Bind(s_count, sizeof(s_count) - 1, 0) && Bind(s_long, sizeof(s_long), 1),
          "messages to format");
    TestCapacity();
    return s_failures == 0 ? 0 : 1;
}
