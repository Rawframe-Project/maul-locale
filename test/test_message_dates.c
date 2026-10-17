// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date and time functions of messages (maul-locale/message.h, record
// mloc-0045): :date, :time and :datetime with the specification's Draft
// options over the library's formatters, their operands (literals and
// date, time and date-time arguments) and refusals, options passed on,
// calendars, and the formatters prepared at bind.

#include "message_prepare.h"
#include "test_harness.h"

#include "maul-locale/message.h"

#include <stdio.h>
#include <string.h>

// The narrow no-break space English writes before AM and PM.
#define NNBSP "\xE2\x80\xAF"

static mlocDataSet s_data;
static mlocMessageFormatter s_en;
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
        mlocBindMessage(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) != mloc_success)
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
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0};
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocInitMessageFormatter(&formatter, &s_data, &locale, &options) != mloc_success)
    {
        return false;
    }
    s_using = &formatter;
    bool good = Formats(message, NULL, 0, text, NULL);
    s_using = &s_en;
    return good;
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
    mlocTime time = {15, 4, 6, 0, 250000000};
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
        mlocBindMessage(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
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
}

int main(void)
{
    static mlocData sources[2];
    alignas(8) static uint8_t dates[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    FILE* file = fopen(MLOC_DATES_FILE, "rb");
    size_t size = file == NULL ? 0 : fread(dates, 1, sizeof dates, file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (size == 0 || size >= sizeof dates || mlocOpenData(&sources[1], dates, size) != mloc_success)
    {
        fputs("test_message_dates: cannot open " MLOC_DATES_FILE "\n", stderr);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    mlocLocale en;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&s_en, &s_data, &en, &options) == mloc_success,
          "a formatter is made");
    TestOptions();
    TestCalendars();
    TestOperands();
    TestPrepared();
    return s_failures == 0 ? 0 : 1;
}
