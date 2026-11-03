// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date and time functions of messages (maul-locale/message.h): :date,
// :time and :datetime with the specification's Draft options over the
// library's formatters, their operands (literals and date, time,
// date-time, instant and zoned arguments) and refusals, options passed on,
// calendars, zones, and the formatters prepared at bind.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "message_prepare.h"
#include "test_harness.h"
#include "zone_tables.h"

#include "maul-locale/message.h"

#include <stdio.h>
#include <string.h>

// The narrow no-break space English writes before AM and PM.
#define NNBSP "\xE2\x80\xAF"

static mlocDataSet s_data;
static mlocMessageFormatter s_en;
// English with New York as its default zone.
static mlocMessageFormatter s_york;
static const mlocMessageFormatter* s_using = &s_en;
static uint8_t s_compiled[1 << 16];
static uint8_t s_bound[1 << 16];
static char s_text[1 << 12];

// Whether a message formats to a text with the faults given in order
// (0 ending the list).
static bool Formats(const char* message, const mlocMessageArgument* arguments, size_t count,
                    const char* text, const mlocMessageFaultKind* expected)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    if (mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocMessageFormatter_Bind(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success)
    {
        printf("  [%s] does not compile and bind\n", message);
        return false;
    }
    mlocMessageFault list[8];
    mlocMessageFaults faults = {list, 8, 0};
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    mlocResult result = mlocFormatMessage(s_bound, arguments, count, &output, &faults);
    size_t expectedCount = 0;
    while (expected != NULL && expected[expectedCount] != mloc_messageFaultNone)
    {
        expectedCount++;
    }
    bool good = result == (expectedCount > 0 ? mloc_withFallbacks : mloc_success) &&
                output.length == strlen(text) && memcmp(s_text, text, output.length) == 0 &&
                faults.count == expectedCount;
    for (size_t i = 0; good && i < expectedCount; i++)
    {
        good = list[i].kind == expected[i];
    }
    if (!good)
    {
        printf("  [%s] -> %d [%.*s], %zu faults", message, (int)result, (int)output.length, s_text,
               faults.count);
        for (size_t i = 0; i < faults.count && i < 8; i++)
        {
            printf(" %d", (int)list[i].kind);
        }
        printf("\n");
    }
    return good;
}

static const mlocMessageFaultKind s_badOption[] = {mloc_messageFaultBadOption, 0};
static const mlocMessageFaultKind s_badOperand[] = {mloc_messageFaultBadOperand, 0};

static void TestOptions(void)
{
    CHECK(Formats("{|2006-01-02| :date} {|2006-01-02| :date length=long} {|2006-01-02| :date "
                  "length=short}",
                  NULL, 0, "Jan 2, 2006 January 2, 2006 1/2/06", NULL),
          ":date's lengths, medium by default");
    CHECK(Formats("{|2006-01-02| :date fields=weekday}|{|2006-01-02| :date fields=day-weekday}|{"
                  "|2006-01-02| :date fields=month-day}|{|2006-01-02| :date "
                  "fields=month-day-weekday length=long}|{|2006-01-02| :date "
                  "fields=year-month-day-weekday length=long}",
                  NULL, 0, "Mon|2 Mon|Jan 2|Monday, January 2|Monday, January 2, 2006", NULL),
          ":date's fields");
    CHECK(Formats("{|2006-01-02T15:04:06| :time}|{|2006-01-02T15:04:06| :time "
                  "precision=second}|{|2006-01-02T15:04:06| :time precision=hour}|{"
                  "|2006-01-02T15:04:06| :time hour12=false}|{|2006-01-02T03:04:06| :time "
                  "hour12=true}",
                  NULL, 0, "3:04" NNBSP "PM|3:04:06" NNBSP "PM|3" NNBSP "PM|15:04|3:04" NNBSP "AM",
                  NULL),
          ":time's precisions and hour12");
    CHECK(Formats("{|2006-01-02T15:04:06| :datetime}|{|2006-01-02T15:04:06| :datetime "
                  "dateLength=long timePrecision=second}|{|2006-01-02T15:04:06| :datetime "
                  "dateFields=weekday hour12=false}",
                  NULL, 0,
                  "Jan 2, 2006, 3:04" NNBSP "PM|January 2, 2006 at 3:04:06" NNBSP "PM|Mon 15:04",
                  NULL),
          ":datetime's options");
    CHECK(Formats("{|2006-01-02| :date calendar=japanese length=long}|{|2006-01-02| :date "
                  "calendar=hebrew length=long}",
                  NULL, 0, "January 2, 18 Heisei|2 Tevet 5766", NULL),
          "a calendar");
    CHECK(
        Formats("{|2006-01-02| :date length=longest}", NULL, 0, "Jan 2, 2006", s_badOption) &&
            Formats("{|2006-01-02| :date calendar=martian}", NULL, 0, "Jan 2, 2006", s_badOption) &&
            Formats("{|2006-01-02T15:04:06| :time hour12=yes}", NULL, 0, "3:04" NNBSP "PM",
                    s_badOption) &&
            Formats("{|2006-01-02| :date precision=hour}", NULL, 0, "Jan 2, 2006", NULL),
        "bad values are bad options; another function's option is ignored");
    mlocMessageArgument given[] = {
        {"l", 1, {mloc_messageValueString, 0, {.string = {"long", 4}}}},
        {"h", 1, {mloc_messageValueString, 0, {.string = {"false", 5}}}},
        {"c", 1, {mloc_messageValueString, 0, {.string = {"japanese", 8}}}},
    };
    CHECK(Formats("{|2006-01-02| :date length=$l}", given, 3, "Jan 2, 2006", s_badOption) &&
              Formats("{|2006-01-02T15:04:06| :time hour12=$h}", given, 3, "15:04", NULL) &&
              Formats("{|2006-01-02| :date calendar=$c}", given, 3, "Jan 2, 18 Heisei", NULL),
          "options other than overrides must be literals; overrides may be variables");
}

// Whether a message formats to a text in a locale.
static bool FormatsIn(const char* tag, const char* message, const char* text)
{
    static mlocMessageFormatter formatter;
    mlocLocale locale;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocInitMessageFormatter(&s_data, &locale, &options, &formatter) != mloc_success)
    {
        return false;
    }
    s_using = &formatter;
    bool good = Formats(message, NULL, 0, text, NULL);
    s_using = &s_en;
    return good;
}

// A message's text with no arguments, into text; its length, or
// SIZE_MAX when it does not format.
static size_t Text(const char* message, char* text, size_t capacity)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    mlocOutput output = {text, capacity, 0, NULL, NULL};
    bool good =
        mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status == mloc_success &&
        mlocMessageFormatter_Bind(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) ==
            mloc_success &&
        mlocFormatMessage(s_bound, NULL, 0, &output, NULL) == mloc_success;
    return good ? output.length : SIZE_MAX;
}

// Whether two messages format alike in a locale.
static bool SameIn(const char* tag, const char* message, const char* other)
{
    static mlocMessageFormatter formatter;
    static char first[256];
    static char second[256];
    mlocLocale locale;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocInitMessageFormatter(&s_data, &locale, &options, &formatter) != mloc_success)
    {
        return false;
    }
    s_using = &formatter;
    size_t one = Text(message, first, sizeof first);
    size_t two = Text(other, second, sizeof second);
    s_using = &s_en;
    if (one == SIZE_MAX || one != two || memcmp(first, second, one) != 0)
    {
        printf("  [%s] in %s: [%.*s], not [%.*s]\n", message, tag, (int)(one == SIZE_MAX ? 0 : one),
               first, (int)(two == SIZE_MAX ? 0 : two), second);
        return false;
    }
    return true;
}

static void TestCalendars(void)
{
    CHECK(FormatsIn("en-u-ca-buddhist",
                    "{|2006-01-02| :date length=long}|{|2006-01-02| :date "
                    "length=long calendar=gregory}",
                    "January 2, 2549 BE|January 2, 2006") &&
              FormatsIn("en-u-ca-buddhist-nu-arab", "{|2006-01-02| :date calendar=gregory}",
                        "Jan \xD9\xA2, \xD9\xA2\xD9\xA0\xD9\xA0\xD9\xA6") &&
              FormatsIn("en-u-nu-arab-x-test", "{|2006-01-02| :date calendar=gregory}",
                        "Jan \xD9\xA2, \xD9\xA2\xD9\xA0\xD9\xA0\xD9\xA6"),
          "the calendar option replaces the locale's, keeping its other keys");
    CHECK(SameIn("en-GB", "{|2006-01-02| :date calendar=gregory}", "{|2006-01-02| :date}") &&
              SameIn("sr-Latn", "{|2006-01-02| :date calendar=gregory length=long}",
                     "{|2006-01-02| :date length=long}") &&
              FormatsIn("en-u-nu-arab-x-test", "{|2006-01-02| :date calendar=buddhist}",
                        "Jan \xD9\xA2, \xD9\xA2\xD9\xA5\xD9\xA4\xD9\xA9 BE") &&
              FormatsIn("en-u-nu-arab-z-abc", "{|2006-01-02| :date calendar=buddhist}",
                        "Jan \xD9\xA2, \xD9\xA2\xD9\xA5\xD9\xA4\xD9\xA9 BE"),
          "the calendar keeps the script, the region and the keys, whatever extensions follow");
    // A tail of 110 bytes, MLOC_MAX_LOCALE_TAIL less 2: -u-ca-buddhist
    // does not fit beside it.
    const char* full = "en-u-nu-arab-a-abcdefgh-abcdefgh-abcdefgh-abcdefgh-abcdefgh-abcdefgh-"
                       "abcdefgh-abcdefgh-abcdefgh-abcdefgh-abcdefgh";
    CHECK(FormatsIn(full, "{|2006-01-02| :date calendar=buddhist}",
                    "Jan \xD9\xA2, \xD9\xA2\xD9\xA5\xD9\xA4\xD9\xA9 BE") &&
              FormatsIn(full, "{|2006-01-02T15:04:06| :time calendar=japanese}",
                        "\xD9\xA3:\xD9\xA0\xD9\xA4" NNBSP "PM") &&
              FormatsIn(full, "{|2006-01-02T15:04:06| :datetime calendar=japanese}",
                        "Jan \xD9\xA2, \xD9\xA1\xD9\xA8 Heisei, \xD9\xA3:\xD9\xA0\xD9\xA4" NNBSP
                        "PM"),
          "the calendar option applies to a locale whose tail has no room for it");
}

static void TestOperands(void)
{
    static const mlocMessageFaultKind unresolved[] = {mloc_messageFaultUnresolvedVariable,
                                                      mloc_messageFaultBadOperand, 0};
    CHECK(Formats("{:date}", NULL, 0, "{:date}", s_badOperand) &&
              Formats("{horse :time}", NULL, 0, "{|horse|}", s_badOperand) &&
              Formats("{|2006-02-30| :date}", NULL, 0, "{|2006-02-30|}", s_badOperand) &&
              Formats("{|15:04| :time}", NULL, 0, "{|15:04|}", s_badOperand) &&
              Formats("{1 :datetime}", NULL, 0, "{|1|}", s_badOperand) &&
              Formats("{$x :date}", NULL, 0, "{$x}", unresolved),
          "no operand, text that is no date, an impossible day, a time alone, a number");
    mlocDate date;
    mlocDateInput input = {NULL, 0, 2006, NULL, 0, 1, 2};
    CHECK(mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &date) == mloc_success,
          "a date is made");
    mlocTime time = {15, 4, 6, 250000000};
    mlocMessageArgument a[] = {
        {"d", 1, {mloc_messageValueDate, 0, {.date = date}}},
        {"t", 1, {mloc_messageValueTime, 0, {.time = time}}},
        {"dt", 2, {mloc_messageValueDateTime, 0, {.dateTime = {date, time}}}},
        {"s", 1, {mloc_messageValueString, 0, {.string = {"2006-01-02T15:04:06", 19}}}},
    };
    CHECK(Formats("{$d :date}|{$d :time}|{$t :time}|{$dt :datetime}|{$s :datetime}", a, 4,
                  "Jan 2, 2006|12:00" NNBSP "AM|3:04" NNBSP "PM|Jan 2, 2006, 3:04" NNBSP
                  "PM|Jan 2, 2006, 3:04" NNBSP "PM",
                  NULL),
          "date, time and date-time arguments, and a string that is a literal");
    CHECK(Formats("{$t :date}", a, 4, "{$t}", s_badOperand) &&
              Formats("{$t :datetime}", a, 4, "{$t}", s_badOperand),
          "a time alone has no date");
    CHECK(Formats("{$d}|{$t}|{$dt}|{$dt :string}", a, 4,
                  "2006-01-02|15:04:06.25|2006-01-02T15:04:06.25|2006-01-02T15:04:06.25", NULL),
          "with no function, or as a string, in ISO 8601");
    CHECK(Formats(".local $d = {|2006-01-02T15:04:06| :datetime hour12=false "
                  "calendar=japanese dateLength=long timePrecision=second} {{{$d :time}|{$d "
                  ":date}}}",
                  NULL, 0, "15:04|Jan 2, 18 Heisei", NULL),
          "a date value passes on its override options alone");
    static const mlocMessageFaultKind selector[] = {mloc_messageFaultBadSelector, 0};
    CHECK(Formats(".local $d = {|2006-01-02| :date} .match $d * {{other}}", NULL, 0, "other",
                  selector),
          "a date value does not select");
    CHECK(Formats(".local $d = {|2006-01-02| :date} {{{$d :string}}}", NULL, 0, "{$d}",
                  s_badOperand) &&
              Formats(".local $u = {$t :time} {{{$u :string}}}", a, 4, "{$u}", s_badOperand),
          "a date value is no string");
}

static void TestZones(void)
{
    CHECK(Formats("{|2006-01-02T15:04:06Z| :time}|{|2006-01-02T15:04:06Z| :time "
                  "timeZone=|America/New_York|}|{|2006-01-02T15:04:06-05:00| :time}|{"
                  "|2006-01-02T15:04:06-05:00| :time timeZone=input}|{|2006-01-02T03:04:06Z| :date "
                  "timeZone=|America/New_York|}",
                  NULL, 0,
                  "3:04" NNBSP "PM|10:04" NNBSP "AM|8:04" NNBSP "PM|3:04" NNBSP "PM|Jan 1, 2006",
                  NULL),
          "an exact time shows in UTC with no default zone, else in the zone asked for");
    CHECK(Formats("{|2006-01-02T15:04:06Z| :time timeZoneStyle=short}|{|2006-01-02T15:04:06Z| "
                  ":time timeZoneStyle=long}|{|2006-07-02T15:04:06Z| :time "
                  "timeZone=|America/New_York| timeZoneStyle=short}",
                  NULL, 0,
                  "3:04" NNBSP "PM UTC|3:04" NNBSP "PM Coordinated Universal Time|11:04" NNBSP
                  "AM EDT",
                  NULL) &&
              Formats("{|2006-01-02T15:04:06Z| :datetime timeZone=|America/Los_Angeles| "
                      "timeZoneStyle=long}",
                      NULL, 0, "Jan 2, 2006, 7:04" NNBSP "AM Pacific Standard Time", NULL),
          "zone styles name the zone at the instant");
    CHECK(Formats("{|2006-01-02T15:04:06-05:00[America/New_York]| :time timeZone=input "
                  "timeZoneStyle=short}|{|2006-01-02T15:04:06[America/New_York]| :time "
                  "timeZone=input}|{|2006-01-02T20:04:06Z[America/New_York]| :time "
                  "timeZone=input}|{|2006-01-02[u-ca=iso8601]| :date}",
                  NULL, 0, "3:04" NNBSP "PM EST|3:04" NNBSP "PM|3:04" NNBSP "PM|Jan 2, 2006", NULL),
          "zone annotations");
    CHECK(Formats("{|2006-01-02T15:04:06+01:00[America/New_York]| :time}", NULL, 0,
                  "{|2006-01-02T15:04:06+01:00[America/New_York]|}", s_badOperand) &&
              Formats("{|2006-01-02T15:04:06[Mars/Olympus]| :time}", NULL, 0,
                      "{|2006-01-02T15:04:06[Mars/Olympus]|}", s_badOperand) &&
              Formats("{|2006-01-02T15:04:06+05:30:15| :time}", NULL, 0,
                      "{|2006-01-02T15:04:06+05:30:15|}", s_badOperand) &&
              Formats("{|2006-01-02[u-ca=hebrew]| :date}", NULL, 0, "{|2006-01-02[u-ca=hebrew]|}",
                      s_badOperand) &&
              Formats("{|2006-01-02[u-ca=persian]| :date}", NULL, 0, "{|2006-01-02[u-ca=persian]|}",
                      s_badOperand) &&
              Formats("{|2006-01-02[u-ca=ISO8601]| :date}", NULL, 0, "Jan 2, 2006", NULL),
          "an offset the zone did not have, an unknown zone, an offset of seconds, a calendar");
    CHECK(Formats("{|2006-01-02T15:04:06| :time timeZoneStyle=short}", NULL, 0, "3:04" NNBSP "PM",
                  s_badOption) &&
              Formats("{|2006-01-02T15:04:06| :time timeZone=input}", NULL, 0, "3:04" NNBSP "PM",
                      s_badOperand) &&
              Formats("{|2006-01-02T15:04:06| :time timeZone=|America/New_York| "
                      "timeZoneStyle=short}|{|2006-07-02T15:04:06| :time "
                      "timeZone=|America/New_York|}",
                      NULL, 0, "3:04" NNBSP "PM EST|3:04" NNBSP "PM", NULL),
          "a floating value: no zone to show, no input zone, or read in the zone given");
    CHECK(Formats("{|2006-01-02T15:04:06Z| :time timeZone=|Mars/Olympus|}", NULL, 0,
                  "3:04" NNBSP "PM", s_badOption) &&
              Formats("{|2006-01-02T15:04:06Z| :time timeZoneStyle=medium}", NULL, 0,
                      "3:04" NNBSP "PM", s_badOption) &&
              Formats("{|2006-01-02T15:04:06Z| :date timeZoneStyle=long}", NULL, 0, "Jan 2, 2006",
                      NULL),
          "bad zone options; :date takes no zone style");
    mlocMessageArgument given[] = {
        {"z", 1, {mloc_messageValueString, 0, {.string = {"Asia/Tokyo", 10}}}},
        {"s", 1, {mloc_messageValueString, 0, {.string = {"short", 5}}}},
    };
    CHECK(
        Formats("{|2006-01-02T15:04:06Z| :time timeZone=$z}", given, 2, "12:04" NNBSP "AM", NULL) &&
            Formats("{|2006-01-02T15:04:06Z| :time timeZoneStyle=$s}", given, 2, "3:04" NNBSP "PM",
                    s_badOption),
        "timeZone may come from a variable, timeZoneStyle may not");
    CHECK(Formats(".local $d = {|2006-01-02T15:04:06Z| :datetime timeZone=|America/New_York| "
                  "timeZoneStyle=long} {{{$d :time}}}",
                  NULL, 0, "10:04" NNBSP "AM", NULL),
          "the zone passes on, its style does not");
}

static void TestDefaultZone(void)
{
    s_using = &s_york;
    CHECK(Formats("{|2006-01-02T15:04:06Z| :time}|{|2006-01-02T15:04:06| :time}|{"
                  "|2006-01-02T15:04:06| :time timeZoneStyle=short}|{|2006-01-02T15:04:06Z| :time "
                  "timeZone=input}|{|2006-01-02T15:04:06+01:00| :time timeZone=input "
                  "timeZoneStyle=short}",
                  NULL, 0,
                  "10:04" NNBSP "AM|3:04" NNBSP "PM|3:04" NNBSP "PM EST|3:04" NNBSP "PM|3:04" NNBSP
                  "PM GMT+1",
                  NULL),
          "the default zone shows exact times and names a floating value's zone");
    s_using = &s_en;
    mlocLocale en;
    mlocMessageFormatter formatter;
    mlocTimeZone bad = {&en, 1, 0, 0};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, &bad};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &options, &formatter) == mloc_errorInvalid,
          "a default zone the library did not make is refused");
}

static void TestZonedArguments(void)
{
    mlocTimeZone york;
    CHECK(mlocFindTimeZone(&s_data, "America/New_York", 16, &york) == mloc_success,
          "a zone is found");
    mlocMessageArgument a[] = {
        {"i", 1, {mloc_messageValueInstant, 0, {.instant = {1136214246, 500000000}}}},
        {"z", 1, {mloc_messageValueZonedDateTime, 0, {.zoned = {{1136214246, 0}, &york}}}},
    };
    CHECK(Formats("{$i :time precision=second}|{$z :time}|{$z :time timeZone=input "
                  "timeZoneStyle=long}|{$i :time timeZone=input timeZoneStyle=short}",
                  a, 2,
                  "3:04:06" NNBSP "PM|3:04" NNBSP "PM|10:04" NNBSP
                  "AM Eastern Standard Time|3:04" NNBSP "PM UTC",
                  NULL),
          "instant and zoned arguments, an instant's zone UTC's");
    CHECK(Formats("{$i}|{$z}|{$z :string}", a, 2,
                  "2006-01-02T15:04:06.5Z|2006-01-02T10:04:06-05:00[America/New_York]|"
                  "2006-01-02T10:04:06-05:00[America/New_York]",
                  NULL),
          "with no function, or as a string, as Temporal writes them");
    mlocTimeZone london;
    CHECK(mlocFindTimeZone(&s_data, "Europe/London", 13, &london) == mloc_success,
          "a zone is found");
    mlocMessageValue zoned = {
        mloc_messageValueZonedDateTime, 0, {.zoned = {{1136214246, 0}, &london}}};
    // "2006-01-02T15:04:06+00:00[Europe/London]": 40 bytes, written in a
    // room of 41 or more; at 39 the identifier fits but not its bracket.
    char text[64];
    char tight[39];
    CHECK(mlocWriteIsoDateTime(&zoned, text, sizeof text) == 40 &&
              memcmp(text, "2006-01-02T15:04:06+00:00[Europe/London]", 40) == 0 &&
              mlocWriteIsoDateTime(&zoned, text, 41) == 40 &&
              mlocWriteIsoDateTime(&zoned, text, 40) == 0 &&
              mlocWriteIsoDateTime(&zoned, tight, sizeof tight) == 0 &&
              mlocWriteIsoDateTime(&zoned, text, 26) == 0,
          "an offset of 0 is +00:00; a zoned text fits its room with its end or is none");
}

alignas(8) static uint8_t s_zones[1 << 20];

static void Put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
    {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

static uint32_t Align8(uint32_t n)
{
    return (n + 7) / 8 * 8;
}

// Writes into s_zones a data file of the compiled-in zones unit with its
// last identifier, Zulu, renamed to Zulu and extra x's: the new name goes
// after the text, and the primaries after it. Returns the file's size, or
// 0, and the name.
static uint32_t BuildLongZone(uint32_t extra, char* nameOut)
{
    static uint8_t s_unit[1 << 20];
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet set = {&builtin, 1};
    DataSpan zones;
    if (!mlocFindDataSetUnit(&set, kindZones, nullptr, 0, &zones))
    {
        return 0;
    }
    static const uint32_t s_sizes[zoneSectionCount] = {
        zoneRecordSize, zoneIdentifierSize, zoneOffsetSize, 2, 4, 8, 1, zoneClosingSize, 1, 2};
    uint32_t starts[zoneSectionCount];
    uint32_t counts[zoneSectionCount];
    uint32_t at = zoneHeaderSize;
    for (int i = 0; i < zoneSectionCount; i++)
    {
        counts[i] = mlocLoad32(zones.bytes + 4 * i);
        starts[i] = at = Align8(at);
        at += counts[i] * s_sizes[i];
    }
    uint32_t text = starts[8];
    uint32_t textSize = counts[8];
    const uint8_t* last = zones.bytes + starts[1] + (counts[1] - 1) * zoneIdentifierSize;
    uint32_t length = last[4] + extra;
    if (length > UINT8_MAX || Align8(text + textSize + length) + 2 * counts[9] > sizeof(s_unit))
    {
        return 0;
    }
    memset(s_unit, 0, sizeof(s_unit));
    memcpy(s_unit, zones.bytes, text + textSize);
    memcpy(s_unit + text + textSize, zones.bytes + text + mlocLoad32(last), last[4]);
    memset(s_unit + text + textSize + last[4], 'x', extra);
    memcpy(nameOut, s_unit + text + textSize, length);
    uint8_t* renamed = s_unit + starts[1] + (counts[1] - 1) * zoneIdentifierSize;
    Put32(renamed, textSize);
    renamed[4] = (uint8_t)length;
    Put32(s_unit + 4 * 8, textSize + length);
    uint32_t primaries = Align8(text + textSize + length);
    memcpy(s_unit + primaries, zones.bytes + starts[9], 2 * counts[9]);
    uint32_t unitSize = primaries + 2 * counts[9];
    uint32_t payload = (uint32_t)(dataHeaderSize + dataUnitSize + 7) / 8 * 8;
    uint32_t size = payload + unitSize;
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memset(s_zones, 0, sizeof(s_zones));
    memcpy(s_zones + headerMagic, "MLOC", 4);
    s_zones[headerFormat] = (uint8_t)dataFormatVersion;
    s_zones[headerFormat + 1] = (uint8_t)(dataFormatVersion >> 8);
    s_zones[headerHeaderSize] = (uint8_t)dataHeaderSize;
    s_zones[headerHeaderSize + 1] = (uint8_t)(dataHeaderSize >> 8);
    Put32(s_zones + headerFileSize, size);
    Put32(s_zones + headerCldr,
          mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch));
    Put32(s_zones + headerUnicode,
          mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch));
    Put32(s_zones + headerDataFormat, version.format);
    // The release is the zones header's last eight bytes, NUL-padded.
    for (int i = 0; i < 8 && s_unit[40 + i] != 0; i++)
    {
        s_zones[headerTzdb + i] = s_unit[40 + i];
    }
    Put32(s_zones + headerTables, (uint32_t)version.tables);
    Put32(s_zones + headerTables + 4, (uint32_t)(version.tables >> 32));
    Put32(s_zones + headerUnitCount, 1);
    Put32(s_zones + headerPoolOffset, size);
    uint8_t* entry = s_zones + dataHeaderSize;
    Put32(entry, kindZones);
    Put32(entry + 4, dataNoLocale);
    Put32(entry + 8, payload);
    Put32(entry + 12, unitSize);
    memcpy(s_zones + payload, s_unit, unitSize);
    return size;
}

// A zoned argument written with no function, its zone's identifier of
// any length a zones unit gives.
static void TestLongZoneIdentifier(void)
{
    char name[UINT8_MAX];
    uint32_t size = BuildLongZone(UINT8_MAX - 4, name);
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[1]);
    mlocDataSet set = {sources, 2};
    mlocTimeZone zone;
    CHECK(size > 0 && memcmp(name, "Zulu", 4) == 0 &&
              mlocOpenData(s_zones, size, &sources[0]) == mloc_success &&
              mlocFindTimeZone(&set, name, sizeof(name), &zone) == mloc_success,
          "a zone of a 255-byte identifier is found");
    mlocMessageArgument z = {
        "z", 1, {mloc_messageValueZonedDateTime, 0, {.zoned = {{1136239445, 123456789}, &zone}}}};
    static char want[512];
    int n = snprintf(want, sizeof want, "2006-01-02T22:04:05.123456789+00:00[%.*s]",
                     (int)sizeof(name), name);
    s_using = &s_en;
    CHECK(n == 292 && Formats("{$z}", &z, 1, want, NULL), "is written whole, 292 bytes");
}

// Whether a format call refuses an argument as mloc_errorInvalid.
static bool Refused(const char* message, const mlocMessageArgument* argument)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    return mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size,
                              &error)
                   .status == mloc_success &&
           mlocMessageFormatter_Bind(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) ==
               mloc_success &&
           mlocFormatMessage(s_bound, argument, 1, &output, NULL) == mloc_errorInvalid;
}

static void TestRefusedArguments(void)
{
    mlocDate date = {0};
    mlocDate dangi = {0};
    mlocDateInput input = {NULL, 0, 2006, NULL, 0, 1, 2};
    CHECK(mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &date) ==
                  mloc_success &&
              mlocMakeDateFromDay(mloc_calendarDangi, date.day, &dangi) == mloc_success,
          "dates are made");
    mlocMessageArgument k = {"d", 1, {mloc_messageValueDate, 0, {.date = dangi}}};
    CHECK(Formats("{$d :date calendar=gregory}", &k, 1, "Jan 2, 2006", NULL),
          "a date of the last calendar is taken");
    mlocTime late = {23, 59, 59, 999999999};
    mlocTime over = {23, 59, 59, 1000000000};
    mlocTime hour = {24, 0, 0, 0};
    mlocTimeZone none = {&date, 1, 0, 0};
    mlocMessageArgument t = {"d", 1, {mloc_messageValueTime, 0, {.time = late}}};
    CHECK(Formats("{$d :time precision=second}", &t, 1, "11:59:59" NNBSP "PM", NULL),
          "the last nanosecond of a day is taken");
    mlocMessageArgument bad[] = {
        {"d", 1, {mloc_messageValueTime, 0, {.time = over}}},
        {"d", 1, {mloc_messageValueDateTime, 0, {.dateTime = {date, hour}}}},
        {"d", 1, {mloc_messageValueInstant, 0, {.instant = {0, 1000000000}}}},
        {"d", 1, {mloc_messageValueInstant, 0, {.instant = {0, -1}}}},
        {"d", 1, {mloc_messageValueInstant, 0, {.instant = {8640000000000, 1}}}},
        {"d", 1, {mloc_messageValueInstant, 0, {.instant = {-8640000000001, 0}}}},
        {"d", 1, {mloc_messageValueZonedDateTime, 0, {.zoned = {{0, 0}, NULL}}}},
        {"d", 1, {mloc_messageValueZonedDateTime, 0, {.zoned = {{0, 0}, &none}}}},
        {"d", 1, {mloc_messageValueZonedDateTime, 0, {.zoned = {{0, 1000000000}, NULL}}}},
    };
    bool refused = true;
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        if (!Refused("{$d :time}", &bad[i]))
        {
            printf("  argument %zu is taken\n", i);
            refused = false;
        }
    }
    CHECK(refused, "a time past a day, an instant out of range, a zone the library did not make");
    mlocMessageArgument edges[] = {
        {"d", 1, {mloc_messageValueInstant, 0, {.instant = {8640000000000, 0}}}},
        {"d", 1, {mloc_messageValueInstant, 0, {.instant = {-8640000000000, 0}}}},
    };
    CHECK(Formats("{$d :date}", &edges[0], 1, "Sep 13, 275760", NULL) &&
              Formats("{$d :date}", &edges[1], 1, "Apr 20, 271822 BC", NULL),
          "Temporal's first and last instants are taken");
}

static void TestIsoText(void)
{
    mlocDate last = {0};
    mlocDate past = {0};
    mlocDateInput input = {NULL, 0, 9999, NULL, 0, 12, 31};
    mlocDateInput next = {NULL, 0, 10000, NULL, 0, 1, 1};
    CHECK(mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &last) == mloc_success &&
              mlocMakeDate(mloc_calendarIso8601, &next, mloc_overflowReject, &past) == mloc_success,
          "dates are made");
    mlocMessageArgument a[] = {
        {"a", 1, {mloc_messageValueDate, 0, {.date = last}}},
        {"b", 1, {mloc_messageValueDate, 0, {.date = past}}},
    };
    CHECK(Formats("{$a}|{$b}", a, 2, "9999-12-31|+010000-01-01", NULL),
          "years past four digits take a sign and six");
    mlocTime t = {15, 4, 6, 250000000};
    mlocMessageValue dateTime = {mloc_messageValueDateTime, 0, {.dateTime = {last, t}}};
    char text[64];
    size_t size = 0;
    size_t written = 0;
    CHECK(mlocWriteIsoDateTime(&dateTime, text, 22) == 0 &&
              mlocWriteIsoDateTime(&dateTime, text, 23) == 22 &&
              memcmp(text, "9999-12-31T15:04:06.25", 22) == 0 &&
              mlocWriteIsoDateTime(&dateTime, text, 11) == 0 &&
              mlocWriteIsoDateTime(&dateTime, text, 12) == 0,
          "an ISO text that does not fit its room with its end is none");
    memset(text, 0, sizeof text);
    CHECK(mlocArgumentText(&dateTime, 0, text, 12, &written, &size) && written == 12 &&
              size == 22 && mlocArgumentText(&dateTime, 12, text + 12, 12, &written, &size) &&
              written == 10 && size == 22 && memcmp(text, "9999-12-31T15:04:06.25", 23) == 0,
          "an argument's ISO text is written a piece at a time");
}

// Whether binding prepared each expression's value as wanted says, in
// order ('d', 't' or 'b' for a date, time or date-time formatter, '-' for
// none).
static bool PreparedAs(const char* message, const char* wanted)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    Resolution r;
    memset(&r, 0, sizeof r);
    if (mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocMessageFormatter_Bind(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success ||
        !mlocReadBound(s_bound, &r.h, &r.formatter) ||
        r.h.view.count[messageExpressions] != strlen(wanted))
    {
        printf("  [%s] does not bind as wanted\n", message);
        return false;
    }
    r.words = mlocBoundWords(s_bound);
    for (uint32_t e = 0; wanted[e] != '\0'; e++)
    {
        const PreparedDate* p = mlocPreparedFor(&r, e);
        char got = p == NULL ? '-' : "dtb"[p->key.function];
        if (got != wanted[e])
        {
            printf("  [%s] expression %u: %c, not %c\n", message, (unsigned)e, got, wanted[e]);
            return false;
        }
    }
    return true;
}

static void TestPrepared(void)
{
    CHECK(PreparedAs("{$d :date} {$d :time} {$d :datetime} {$d :date length=$l} {$d :time "
                     "hour12=$h}",
                     "dtb--"),
          "date functions are prepared, unless an option comes from an argument");
    mlocMessageArgument d = {"d", 1, {mloc_messageValueString, 0, {.string = {"2006-01-02", 10}}}};
    CHECK(Formats("{$d :date length=long}", &d, 1, "January 2, 2006", NULL),
          "a prepared formatter writes as one made in the call");
    CHECK(PreparedAs("{$d :time timeZoneStyle=short} {$d :datetime timeZone=$z} {$d :time "
                     "timeZone=|Asia/Tokyo|}",
                     "t-t"),
          "zone options are prepared, unless they come from an argument");
    mlocMessageArgument a[] = {
        {"f", 1, {mloc_messageValueString, 0, {.string = {"2006-01-02T15:04:06", 19}}}},
        {"i", 1, {mloc_messageValueInstant, 0, {.instant = {1136214246, 0}}}},
    };
    CHECK(Formats("{$i :time timeZoneStyle=short}", a, 2, "3:04" NNBSP "PM UTC", NULL) &&
              Formats("{$f :time timeZoneStyle=short}", a, 2, "3:04" NNBSP "PM", s_badOption),
          "a value whose options differ from the prepared ones has a formatter of its own");
}

// Opens a data file into its own buffer as a source; false when it
// cannot.
static bool Open(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t size = file == NULL ? 0 : fread(buffer, 1, capacity, file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (size == 0 || size >= capacity || mlocOpenData(buffer, size, out) != mloc_success)
    {
        fprintf(stderr, "test_message_dates: cannot open %s\n", path);
        return false;
    }
    return true;
}

int main(void)
{
    static mlocData sources[4];
    alignas(8) static uint8_t dates[8 << 20];
    alignas(8) static uint8_t zoneNames[8 << 20];
    alignas(8) static uint8_t names[16 << 20];
    mlocOpenBuiltinData(&sources[0]);
    if (!Open(MLOC_DATES_FILE, dates, sizeof dates, &sources[1]) ||
        !Open(MLOC_ZONE_NAMES_FILE, zoneNames, sizeof zoneNames, &sources[2]) ||
        !Open(MLOC_LOCALE_NAMES_FILE, names, sizeof names, &sources[3]))
    {
        return 2;
    }
    s_data = (mlocDataSet){sources, 4};
    mlocLocale en;
    mlocTimeZone york;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    mlocMessageOptions inYork = {false, mloc_messageIsolationNone, NULL, 0, &york};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &options, &s_en) == mloc_success &&
              mlocFindTimeZone(&s_data, "America/New_York", 16, &york) == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &inYork, &s_york) == mloc_success,
          "the formatters are made");
    TestOptions();
    TestCalendars();
    TestOperands();
    TestZones();
    TestDefaultZone();
    TestZonedArguments();
    TestLongZoneIdentifier();
    TestRefusedArguments();
    TestIsoText();
    TestPrepared();
    return s_failures == 0 ? 0 : 1;
}
